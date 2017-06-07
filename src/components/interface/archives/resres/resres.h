#ifndef RESRES_H
#define RESRES_H

#include <res_spec.h>

res_t resres_create(spdid_t spd, spdid_t dest, res_type_t t, res_hardness_t h);
int resres_bind(spdid_t spd, res_t r, res_spec_t ra);
res_spec_t resres_notif(spdid_t spd, res_t r, res_spec_t ra);
int resres_delete(spdid_t spd, res_t r);

#endif
