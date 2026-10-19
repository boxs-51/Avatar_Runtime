"""Required native targets must never fall back to another build or skip green."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
SCRIPTS=(('ARS_MOC3_PHYSICS_TEST_EXE','test_native_physics_reference.py'),
         ('ARS_MOC3_PUBLICATION_HOST_EXE','test_packet_publication.py'),
         ('ARS_MOC3_PUBLICATION_HOST_EXE','test_step_publication.py'))

@unittest.skipUnless(sys.platform=='win32','native publication targets require Windows')
class ConfiguredTargets(unittest.TestCase):
    def test_missing_required_target_is_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            for key,script in SCRIPTS:
                with self.subTest(script=script):
                    env=dict(os.environ);env[key]=str(Path(folder)/'missing-required.exe')
                    result=subprocess.run([sys.executable,'-B',str(ROOT/'tests/moc3'/script)],env=env,capture_output=True,timeout=40)
                    self.assertNotEqual(result.returncode,0,'registered required target must not skip or use a default build')

    def test_two_builds_and_configurations_launch_selected_target(self):
        self.assertIsNotNone(shutil.which('cmake'),'CMake required for target-custody verification')
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder);source=folder/'source';source.mkdir()
            (source/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(DispatchSentinel LANGUAGES CXX)
add_executable(sentinel sentinel.cpp)
target_compile_definitions(sentinel PRIVATE DISPATCH_EXIT=${DISPATCH_EXIT})
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/target-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:sentinel>")
''',encoding='utf-8')
            (source/'sentinel.cpp').write_text('''#include <cstdio>
#include <cstdlib>
int main(){const auto p=std::getenv("ARS_DISPATCH_MARKER");
if(p){if(auto f=std::fopen(p,"wb")){std::fprintf(f,"%d",DISPATCH_EXIT);std::fclose(f);}}
return DISPATCH_EXIT;}
''',encoding='utf-8')
            for name,config,code in (('build A','Release',73),('build B','Debug',74)):
                build=folder/name
                configured=subprocess.run(['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE='+config,'-DDISPATCH_EXIT='+str(code)],capture_output=True,timeout=90)
                self.assertEqual(configured.returncode,0,configured.stderr.decode(errors='replace'))
                compiled=subprocess.run(['cmake','--build',str(build),'--config',config],capture_output=True,timeout=90)
                self.assertEqual(compiled.returncode,0,compiled.stderr.decode(errors='replace'))
                executable=(build/('target-'+config+'.txt')).read_text(encoding='utf-8')
                for key,script in SCRIPTS:
                    with self.subTest(build=name,config=config,script=script):
                        marker=build/(script+'.marker');env=dict(os.environ)
                        env.update({key:executable,'ARS_DISPATCH_MARKER':str(marker)})
                        result=subprocess.run([sys.executable,'-B',str(ROOT/'tests/moc3'/script)],env=env,capture_output=True,timeout=40)
                        self.assertNotEqual(result.returncode,0,'selected failing target must make required test fail')
                        self.assertTrue(marker.is_file(),'configured target was not actually launched')
                        self.assertEqual(marker.read_text(),str(code))

if __name__=='__main__':unittest.main()
