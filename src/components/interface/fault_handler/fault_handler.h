#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <cos_kernal_api.h>

void fault_div_by_zero(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_memory_access(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_breakpoint(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_invalid_inst(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_invstk(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_comp_not_exist(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);
void fault_handler_not_exist(unsigned long sp, unsigned long ip, unsigned long fault_addr, unsigned long fault_type);

#endif /* FAULT_HANDLER_H */
