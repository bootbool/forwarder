# Ebpf engine

As Ebpf runs in kernel environment, it can bring the benefits of Zero-Copy performance advantage.
Ebpf program is attached to TC ingress hook, that is logically divided into two parts, including controller and redirector.  Conntroller is responsible for synchronizing connection pairs between kernel and userspace, while redirector forward packets to the right net index according to connection pairs mapping.
The flow chart is illustrated as the following.

<img src="ebpf.svg"/>
