#ifndef foosockethfoo
#define foosockethfoo

int flx_open_socket(int iface);


int flx_send_packet(int fd, int iface, struct flx_dns_packet *p);



#endif
