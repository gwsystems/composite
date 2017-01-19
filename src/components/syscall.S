                AREA            ARCH,CODE,READONLY,ALIGN=3

                THUMB
                REQUIRE8
                PRESERVE8

                EXPORT          _Sys_Schedule_Trigger

_Sys_Schedule_Trigger
                PUSH       {R0-R1}

                ;The NVIC_INT_CTRL register
                LDR        R0,=0xE000ED04
                ;Trigger the PendSV
                LDR        R1,=0x10000000
                STR        R1,[R0]

                POP        {R0-R1}
                BX         LR

                END
