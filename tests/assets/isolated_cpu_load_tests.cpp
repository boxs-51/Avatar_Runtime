#include "../../src/assets/isolated_cpu_load.hpp"
#include "asset_test_support.hpp"
#include <chrono>
#include <thread>
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    auto bytes=test::file(argv[1]);test::Host host;
    {
        ars::assets::Resource memory(test::Host::allocate,test::Host::release,&host,256*ars::assets::MiB);
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        std::unique_ptr<ars::assets::AdmissionCustody> custody;
        auto status=ars::assets::isolated_cpu_load(bytes,memory,model,custody);
        std::printf("status=%d pending=%d model=%d\n",status,bool(custody),bool(model));
        test::check(status==(std::string_view(argv[2])=="INTERNAL"?ARS_E_INTERNAL:test::expected_status(argv[2])),"trusted adjacent native helper admission status");
        test::check(bool(model)==(status==ARS_OK),"success model only");
        if(custody) {
            for(unsigned i=0;i<100 && !custody->reap();++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
            test::check(custody->reap(),"actual deferred OS custody cleanup");custody.reset();
        }
    }
    test::check(host.live==0,"all admission allocations freed");
    std::puts("PASS fixed-helper native admission and actual OS cleanup");
}
