#include "app_manager.h"
#include <string.h>

#define VALID 1
#define SELECTED 2
#define TIMEOUT 4
#define ID_READY 64

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static int live(const struct et100_manager_entry *e)
{
    return e->used && !(e->flags & TIMEOUT);
}
static void clear_selection(struct et100_manager *m)
{
    if(m->selected<ET100_MANAGER_DEVICES) m->entries[m->selected].flags &= ~SELECTED;
    m->selected=0xff;
}
void et100_manager_init(struct et100_manager *m)
{
    memset(m,0,sizeof(*m));m->selected=0xff;
}
void et100_manager_expire(struct et100_manager *m, uint32_t now)
{
    unsigned changed=0;
    for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++) {
        struct et100_manager_entry *e=&m->entries[i];
        if(live(e) && (uint32_t)(now-e->seen_ms)>5000) {
            e->flags |= TIMEOUT;
            if(m->selected==i) clear_selection(m);
            changed=1;
        }
    }
    if(changed) {m->list_sequence++;m->event_sequence++;}
}
void et100_manager_update(struct et100_manager *m, const uint8_t mac[6], uint8_t type,
    int8_t rssi, uint8_t adv_type, const uint8_t *raw, size_t length,
    const struct et100_adv_details *details, uint32_t now)
{
    if(!raw || length>31 || !details || !details->version || !details->identity_ready) return;
    et100_manager_expire(m,now);
    unsigned slot=ET100_MANAGER_DEVICES;
    for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++)
        if(m->entries[i].used && m->entries[i].address_type==type &&
           !memcmp(m->entries[i].mac,mac,6)) {slot=i;break;}
    if(slot==ET100_MANAGER_DEVICES && details->version==1)
        for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++)
            if(m->entries[i].used && !memcmp(m->entries[i].identity,details->identity,6)) {slot=i;break;}
    if(slot==ET100_MANAGER_DEVICES)
        for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++)
            if(!live(&m->entries[i])) {slot=i;memset(&m->entries[i],0,sizeof(m->entries[i]));break;}
    if(slot==ET100_MANAGER_DEVICES) return;
    struct et100_manager_entry *e=&m->entries[slot];
    if(!live(e)) m->list_sequence++;
    e->used=1;e->flags=VALID|ID_READY|(m->selected==slot?SELECTED:0);
    memcpy(e->mac,mac,6);e->address_type=type;e->rssi=(uint8_t)rssi;e->adv_type=adv_type;
    memcpy(e->identity,details->identity,6);e->version=details->version;
    e->mode=details->mode;e->role_state=details->state;memcpy(e->firmware,details->firmware,3);
    memcpy(e->raw,raw,length);e->length=(uint8_t)length;e->seen_ms=now;e->sequence++;
    m->event_sequence++;
}
size_t et100_manager_list(struct et100_manager *m, uint32_t now, uint8_t *out, size_t capacity)
{
    if(!out || capacity<4) return 0;
    et100_manager_expire(m,now);
    size_t n=4;uint8_t count=0;
    for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++) {
        const struct et100_manager_entry *e=&m->entries[i];
        if(!live(e)) continue;
        if(capacity-n<16) break;
        uint8_t *p=out+n;p[0]=(uint8_t)i;p[1]=e->address_type;memcpy(p+2,e->mac,6);
        p[8]=e->rssi;p[9]=e->version;p[10]=e->mode;p[11]=e->role_state;
        memcpy(p+12,e->firmware,3);p[15]=e->flags;n+=16;count++;
    }
    out[0]=5;out[1]=count;out[2]=16;out[3]=(uint8_t)m->list_sequence;return n;
}
size_t et100_manager_status(struct et100_manager *m, uint32_t now, uint8_t *out, size_t capacity)
{
    if(!out || capacity<16) return 0;
    et100_manager_expire(m,now);
    unsigned count=0;for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++) count+=live(&m->entries[i]);
    out[0]=1;out[1]=0;out[2]=(uint8_t)count;out[3]=m->selected;
    put32(out+4,now/1000);put32(out+8,m->list_sequence);put32(out+12,m->event_sequence);return 16;
}
size_t et100_manager_selected(struct et100_manager *m, uint32_t now, uint8_t *out, size_t capacity)
{
    if(!out || capacity<6) return 0;
    et100_manager_expire(m,now);
    if(m->selected>=ET100_MANAGER_DEVICES) {
        out[0]=1;out[1]=0xff;put32(out+2,0);return 6;
    }
    const struct et100_manager_entry *e=&m->entries[m->selected];
    size_t n=23+e->length;
    if(capacity<n) return 0;
    out[0]=1;out[1]=m->selected;put32(out+2,e->sequence);out[6]=e->rssi;
    out[7]=e->address_type;memcpy(out+8,e->mac,6);out[14]=e->adv_type;
    out[15]=out[16]=0xff;out[17]=e->flags;put32(out+18,e->seen_ms);
    out[22]=e->length;memcpy(out+23,e->raw,e->length);return n;
}
int et100_manager_select(struct et100_manager *m, uint32_t now, const uint8_t *data, size_t n)
{
    et100_manager_expire(m,now);
    if(!data || n<2 || data[0]!=1) return 0;
    unsigned slot=ET100_MANAGER_DEVICES;
    if(data[1]==2 && n==2) {clear_selection(m);m->list_sequence++;m->event_sequence++;return 1;}
    if(data[1]!=1) return 0;
    if(n==4 && data[2]==0) slot=data[3];
    else if(n==10 && data[2]==1)
        for(unsigned i=0;i<ET100_MANAGER_DEVICES;i++)
            if(live(&m->entries[i]) && m->entries[i].address_type==data[3] &&
               !memcmp(m->entries[i].mac,data+4,6)) {slot=i;break;}
    if(slot>=ET100_MANAGER_DEVICES || !live(&m->entries[slot])) return 0;
    clear_selection(m);m->selected=(uint8_t)slot;m->entries[slot].flags|=SELECTED;
    m->list_sequence++;m->event_sequence++;return 1;
}
