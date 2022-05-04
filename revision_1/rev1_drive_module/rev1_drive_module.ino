/**
 * @file rev1_drive_module.ino
 * 
 * @author Joseph Telaak
 * 
 * @brief This module controls the golf cart's motor control unit.
 *          It is responsible for switching between forward and reverse.
 *          It reports input from the accelerator pedal and ouputs a potentiometer
 *          signal to the motor control unit to act as the "pedal"
 * 
 * @version 0.1
 * 
 * @date 2022-04-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "mcp2515.h"
#include "mcp4xxx.h"

#define ACCEL_CS 9
#define ACT_SW 4
#define FWD_REV_SEL 5
#define PEDAL_POT A3
#define PEDAL_SW 3

#define CAN_CS 10
#define CAN_INT 2

#define CAN_ID 0x003
#define CAN_DLC 8

#define COM_LED A0
#define ACT_LED A1

using namespace icecave::arduino;

MCP4XXX* accel;
MCP2515 can(CAN_CS);

volatile int wiper_pos = 0;
volatile bool pedal_pressed = false;

void setup() {
    pinMode(ACT_LED, OUTPUT);
    digitalWrite(ACT_LED, HIGH);

    can.reset();
    can.setBitrate(CAN_125KBPS);
    can.setNormalMode();

    pinMode(ACT_SW, OUTPUT);
    pinMode(FWD_REV_SEL, OUTPUT);

    pinMode(PEDAL_POT, INPUT);

    pinMode(COM_LED, OUTPUT);

    accel = new MCP4XXX(ACCEL_CS);

    pot_write(0);
    get_wiper_pos();

    digitalWrite(ACT_LED, LOW);

    attachInterrupt(digitalPinToInterrupt(CAN_INT), can_irq, FALLING);
    attachInterrupt(digitalPinToInterrupt(PEDAL_SW), pedal_act, RISING);

}

int counter = 0;

void loop() {
    digitalWrite(ACT_LED, HIGH);

    if (counter % 10 == 0) { get_direc(); get_en_status(); } 
    if (counter % 2 == 0) { get_wiper_pos(); }

    if (pedal_pressed) { 
        get_pedal_pos(); 
        digitalWrite(ACT_LED, LOW);

        delay(10);
        
    } else {
        digitalWrite(ACT_LED, LOW);

        counter++;
        delay(1000);

    }
}

void can_irq() {
    struct can_frame can_msg_in;

    if (can.readMessage(&can_msg_in) == MCP2515::ERROR_OK) {
        digitalWrite(ACT_LED, HIGH);

        if (can_msg_in.can_id == CAN_ID) {
            if (can_msg_in.data[0] == 0x0A) {
                if (can_msg_in.data[1] == 0x0A) {
                    if (can_msg_in.data[2] == 0x0A) {
                        pot_write(can_msg_in.data[3]);
                        get_wiper_pos();

                    } else if (can_msg_in.data[2] == 0x0E) {
                        if (can_msg_in.data[3] == 0x01)
                            digitalWrite(ACT_SW, LOW);
                        else if (can_msg_in.data[3] == 0x02) 
                            digitalWrite(ACT_SW, HIGH);

                        get_en_status();

                    }

                } else if (can_msg_in.data[1] == 0x0D) {
                    if (can_msg_in.data[2] == 0x01) 
                        digitalWrite(FWD_REV_SEL, LOW);
                    else if (can_msg_in.data[2] == 0x02) 
                        digitalWrite(FWD_REV_SEL, HIGH);

                    get_direc();

                }

            }
        } else if (can_msg_in.data[0] == 0x0B) {
            if (can_msg_in.data[1] == 0x0A) {
                if (can_msg_in.data[2] == 0x01) 
                    pot_inc();
                else if (can_msg_in.data[2] == 0x02) 
                    pot_dec();

                get_wiper_pos();

            }

        } else if (can_msg_in.data[0] == 0x0C) {
            if (can_msg_in.data[1] == 0x0A) {
                if (can_msg_in.data[2] == 0x0A) 
                    get_wiper_pos();
                else if (can_msg_in.data[2] == 0x0D) 
                    get_pedal_pos();
                else if (can_msg_in.data[2] == 0x0E)
                    get_en_status();
                

            } else if (can_msg_in.data[1] == 0x0D) {
                get_direc();

            }
        }

        digitalWrite(ACT_LED, LOW);
    }
}

void pot_write(int pos) {
    while (pos != wiper_pos) {
        if (pos > wiper_pos)
            pot_inc();
        else if (pos < wiper_pos)
            pot_dec();
    }

    get_wiper_pos();

}

void pot_inc() { accel -> increment(); wiper_pos++; }
void pot_dec() { accel -> decrement(); wiper_pos--; }

void get_wiper_pos() {
    digitalWrite(COM_LED, HIGH);

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0A;
    can_msg_out.data[3] = 0x0A;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = wiper_pos >> 8;
    can_msg_out.data[7] = wiper_pos & 0xFF;

    can.sendMessage(&can_msg_out);
    digitalWrite(COM_LED, LOW);
    
}

void get_pedal_pos() {
    digitalWrite(COM_LED, HIGH);

    int pedal_pos = analogRead(PEDAL_POT);

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0A;
    can_msg_out.data[3] = 0x0D;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = pedal_pos >> 8;
    can_msg_out.data[7] = pedal_pos & 0xFF;

    can.sendMessage(&can_msg_out);
    digitalWrite(COM_LED, LOW);
    
}

void get_en_status() {
    digitalWrite(COM_LED, HIGH);

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0A;
    can_msg_out.data[3] = 0x0E;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = 0;
    can_msg_out.data[7] = digitalRead(ACT_SW) == LOW ? 0x01 : 0x02;

    can.sendMessage(&can_msg_out);
    digitalWrite(COM_LED, LOW);

}

void get_direc() {
    digitalWrite(COM_LED, HIGH);

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0D;
    can_msg_out.data[3] = 0;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = 0;
    can_msg_out.data[7] = digitalRead(ACT_SW) == LOW ? 0x01 : 0x02;

    can.sendMessage(&can_msg_out);
    digitalWrite(COM_LED, LOW);

}

void pedal_act() {
    noInterrupts();

    digitalWrite(COM_LED, HIGH);

    pedal_pressed = true;

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0E;
    can_msg_out.data[3] = 0;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = 0;
    can_msg_out.data[7] = 0x02;

    can.sendMessage(&can_msg_out);
    interrupts();
    attachInterrupt(digitalPinToInterrupt(PEDAL_SW), pedal_deact, FALLING);

    digitalWrite(COM_LED, LOW);

}

void pedal_deact() {
    noInterrupts();

    digitalWrite(COM_LED, HIGH);

    pedal_pressed = false;

    struct can_frame can_msg_out;

    can_msg_out.can_id = CAN_ID;
    can_msg_out.can_dlc = CAN_DLC;
    can_msg_out.data[0] = 0x0C;
    can_msg_out.data[1] = 0x0C;
    can_msg_out.data[2] = 0x0E;
    can_msg_out.data[3] = 0;
    can_msg_out.data[4] = 0;
    can_msg_out.data[5] = 0;
    can_msg_out.data[6] = 0;
    can_msg_out.data[7] = 0x01;

    can.sendMessage(&can_msg_out);
    interrupts();
    attachInterrupt(digitalPinToInterrupt(PEDAL_SW), pedal_act, RISING);

    digitalWrite(COM_LED, LOW);

}
