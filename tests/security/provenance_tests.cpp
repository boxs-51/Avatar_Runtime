#include "runtime_test_support.hpp"
#include <thread>
#include <stdexcept>

struct Capture {
    test::Authority *owner=nullptr,*other=nullptr;
    ars_source_admin_h source{},other_source{};
    ars_ingress_cap cap{},other_cap{};
    ars_status nested=ARS_OK,independent=ARS_E_INTERNAL;
    std::uint64_t now=200;
    bool mutate=false,throw_error=false;
    test::Gate *gate=nullptr;
    static std::uint64_t ARS_CALL read(void *data) {
        auto &c=*static_cast<Capture *>(data);
        if(c.gate) c.gate->arrive_and_wait();
        if(c.mutate) {
            ars_ingress_cap out{};
            c.nested=c.owner->router.rotate_source(c.owner->runtime,c.source,&out);
            if(c.other) c.independent=c.other->router.rotate_source(c.other->runtime,c.other_source,&out);
        }
        if(c.throw_error) throw std::runtime_error("capture");
        return c.now;
    }
};
int main(int argc,char **) {
    Capture capture; test::Authority a(Capture::read,&capture); capture.owner=&a;
    test::status(ARS_OK,a.register_source(&capture.source,&capture.cap),"P2-register");
    test::Frame frame; frame.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
    if(argc==1) {
        capture.mutate=true;
        (void)a.router.submit_frame(a.runtime,capture.cap,&frame.value);
        test::status(ARS_E_BUSY,capture.nested,"P2-all-mutating-callback-reentry-denied");
    } else {
        test::status(ARS_OK,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-capture-200");
        capture.now=100; frame.value.sequence=2;
        test::status(ARS_E_CLOCK_REGRESSION,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-capture-regression-rejected");
    }
}
