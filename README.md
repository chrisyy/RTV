# RTV
RTV is designed to be a light-weight real-time hypervisor. Being a type-1
bare-metal hypervisor, it manages to keep its memory footprint small by
skipping most of the device drivers. Guest OS provides the necessary drivers
and gets device interrupts directly from the hardware, bypassing the
hypervisor layer.

Currently, RTV can boot up a VM that contains simple print instructions,
showing "OK" on the screen. Booting Linux VM is work-in-progress.

Potential applications:  
Enabling virtualization on real-time embedded platforms;  
Serving as an endpoint security monitor for attack detection/prevention;  
Serving as a Trusted Execution Environment (TEE) for encryption/verification.
