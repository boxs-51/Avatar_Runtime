#include "asset_test_support.hpp"
#include "../../src/validation/schema_worker_client.hpp"
int main(int argc,char** argv){
    test::check(argc==2,"worker executable argument");
    test::Host host;ars::assets::Resource resource(test::Host::allocate,test::Host::release,&host);
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    // Frozen rig schema, semantically trivial but structurally valid.
    const auto text=R"({"schema_version":"0.1.0","coordinate_space":"canvas_px","canvas":{"width":1,"height":1},"parameters":[],"parts":[{"id":"p","z_index":0,"atlas_rect_px":[0,0,1,1],"vertices_px":[[0,0],[1,0],[0,1]],"uv":[[0,0],[1,0],[0,1]],"triangles":[[0,1,2]]}]})";
    auto status=client.validate(ars::validation::Schema::rig,text,resource);
    std::printf("native schema actual=%u expected=%u\n",unsigned(status),unsigned(ARS_OK));
    test::check(status==ARS_OK,"bounded production schema helper validation");
    test::check(resource.live()==0&&host.live==0,"schema lifetime balanced");
}
