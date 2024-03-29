/*
 *        +----------------------------------------------------------+
 *        | +------------------------------------------------------+ |
 *        | |  Quafios Kernel 1.0.2.                               | |
 *        | |  -> i386: process operations.                        | |
 *        | +------------------------------------------------------+ |
 *        +----------------------------------------------------------+
 *
 * This file is part of Quafios 1.0.2 source code.
 * Copyright (C) 2014  Mostafa Abd El-Aziz Mohamed.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quafios.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Visit http://www.quafios.com/ for contact information.
 *
 */

#include <arch/type.h>
#include <sys/proc.h>
#include <sys/scheduler.h>
#include <sys/error.h>

#include <i386/stack.h>
#include <i386/asm.h>

void umode_jmp(int32_t vaddr, int32_t sp) {
    /* used by: exec(). */
    __asm__("pushl $0x2B; # 0x28 + 3 (user mode) \n \
             pushl %%ebx;                        \n \
             pushf      ;                        \n \
             pushl $0x23; # 0x20 + 3 (user mode) \n \
             pushl %%eax;                        \n \
             pushw $0x2B;                        \n \
             pushw $0x2B;                        \n \
             pushw $0x2B;                        \n \
             pushw $0x2B;                        \n \
             popw   %%ds;                        \n \
             popw   %%es;                        \n \
             popw   %%fs;                        \n \
             popw   %%gs;                        \n \
             iret       ; "::"a"(vaddr), "b"(sp));
}

void copy_context(proc_t *child) {
    /* store the context of current process into child.
     * used by: fork().
     */
    Regs *regs = (Regs *) child->kstack;
    *regs = *((Regs *) curproc->context);
    child->context = (void *) regs;
    child->reg1 = (uint32_t) regs;
    child->reg2 = (uint32_t) regs;
    regs->esp = (uint32_t) &(regs->err);
    regs->eax = 0; /* 0 should be returned to the child. */
    return;
}

void arch_proc_switch(proc_t *oldproc, proc_t *newproc) {
    /* switch between two processes! */

    /* update Task Segment: */
    ts.esp0 = (uint32_t) &newproc->kstack[KERNEL_STACK_SIZE];

    /* update CR3: */
    arch_vmswitch(&(newproc->umem));

    /* store stack parameters: */
    __asm__("mov %%ebp, %%eax":"=a"(oldproc->reg1));
    __asm__("mov %%esp, %%eax":"=a"(oldproc->reg2));

    /* retrieve stack parameters of the new process: */
    if (!(newproc->after_fork)) {
        /* get stack parameters: */
        int32_t ebp = newproc->reg1;
        int32_t esp = newproc->reg2;
        __asm__("mov %%eax, %%ebp; \n\
                 mov %%ebx, %%esp;"::"a"(ebp), "b"(esp));
        return;
    }

    /* special case: a process that has just forked: */
    newproc->after_fork = 0;

    /* Send End of Interrupt Command (very tricky): */
    end_irq0();

    /* get the context of the new process */
    __asm__("mov %%eax, %%esp"::"a"(newproc->reg2));

    /* Clear interrupt flag temporarily. */
    cli();

    /* return */
    restore_reg();
    __asm__("add $4, %esp");
    iret();
}
