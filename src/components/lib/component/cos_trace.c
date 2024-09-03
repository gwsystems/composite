#include <cos_trace.h>

struct trace {
    const char *format;
    cycles_t tsc; 
    long cpu;
    thdid_t tid;
    compid_t cid;
    dword_t a, b, c;
};

struct trace_ring_buf {
    struct trace traces[COS_TRACE_NEVENTS];
    struct ck_ring ring;
};

CK_RING_PROTOTYPE(trace_buffer, trace);

struct trace_ring_buf trace_buffer;

// TODO: Constructor seems to be not working, check it later cos_iniit
void CCTOR 
cos_trace_init()
{
    struct trace_ring_buf *ring_buf = &trace_buffer;
    ck_ring_init(&ring_buf->ring, COS_TRACE_NEVENTS);
}

void
cos_trace(const char *format, cycles_t tsc, long cpu, thdid_t tid, compid_t cid, dword_t a, dword_t b, dword_t c)
{
    struct trace new_trace;
    struct trace_ring_buf *ring_buf = &trace_buffer;
    bool res = true;

    assert(&ring_buf->ring);

    new_trace.format = format;
    new_trace.tsc = tsc;
    new_trace.cpu = cpu;
    new_trace.tid = tid;
    new_trace.cid = cid;
    new_trace.a = a;
    new_trace.b = b;
    new_trace.c = c;
    
    res = CK_RING_ENQUEUE_MPSC(trace_buffer, &ring_buf->ring, ring_buf->traces, &new_trace);

    if (!res) {
        unsigned int number_of_traces = ck_ring_size(&ring_buf->ring);
        printc("Trace buffer enqueue failed %d, # of traces: %d\n", res, number_of_traces);
       // assert(res);
    }

}

void
cos_trace_print_buffer()
{
    struct trace_ring_buf *ring_buf = &trace_buffer;
    struct trace trace = {0};
    bool res = true;
    unsigned int i, number_of_traces;

    assert(&ring_buf->ring);

    number_of_traces = ck_ring_size(&ring_buf->ring);

    for (i = 0; i < number_of_traces; i++) {
        res = CK_RING_DEQUEUE_MPSC(trace_buffer, &ring_buf->ring, ring_buf->traces, &trace);
        assert(res);
        
        if (res) {
            printc("[%-11llu|CPU: %ld,TID:%lu,CID:%lu] ", trace.tsc, trace.cpu, trace.tid, trace.cid);
            printc( (char*)trace.format, trace.a, trace.b, trace.c);
        }
    }
}
