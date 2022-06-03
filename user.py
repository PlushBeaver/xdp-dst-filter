from ipaddress import ip_address
from socket import htons, htonl
from bcc import BPF

device = 'xdp0'
flags = 0
b = BPF('kernel.c')
fn = b.load_func("xdp_main", BPF.XDP)
b.attach_xdp(device, fn, flags)
try:
    blocklist = b.get_table("blocklist")

    ip = ip_address('10.9.0.1')
    port = 3000
    
    key = (htons(port) << 32) | htonl(int(ip))
    value = 0

    keys = (blocklist.Key * 1)()
    values = (blocklist.Leaf * 1)()
    keys[0] = blocklist.Key(key)
    values[0] = blocklist.Leaf(value)
    blocklist.items_update_batch(keys, values)

    input()
finally:
    b.remove_xdp(device, flags);
