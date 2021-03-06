//
//    Copyright (C) 2013-2014 Michael Geszkiewicz
//    Copyright (C) 2018 Sebastian Kuzminsky
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//


#include <stdio.h>
#include <string.h>
#include "hostmot2.h"


void hm2_read_idrom(hostmot2_t *hm2) {
    u32 idrom_addr, cookie;
    int i;

    hm2->llio->read(hm2->llio, HM2_COOKIE_REG, &(cookie), sizeof(u32));
    if (cookie != HM2_COOKIE) {
        printf("ERROR: no HOSTMOT2 firmware found. %X\n", cookie);
        return;
    }
    // check if it was already read
    if (strncmp(hm2->config_name, "HOSTMOT2", 8) == 0)
        return;

    hm2->llio->read(hm2->llio, HM2_CONFIG_NAME, &(hm2->config_name), HM2_CONFIG_NAME_LEN);
    hm2->llio->read(hm2->llio, HM2_IDROM_ADDR, &(idrom_addr), sizeof(u32));
    hm2->llio->read(hm2->llio, idrom_addr, &(hm2->idrom), sizeof(hm2->idrom));
    for (i = 0; i < HM2_MAX_MODULES; i++) {
        hm2->llio->read(hm2->llio, idrom_addr + hm2->idrom.offset_to_modules + i*sizeof(hm2_module_desc_t),
          &(hm2->modules[i]), sizeof(hm2_module_desc_t));
    }
    for (i = 0; i < HM2_MAX_PINS; i++) {
        hm2->llio->read(hm2->llio, idrom_addr + hm2->idrom.offset_to_pins + i*sizeof(hm2_pin_desc_t),
          &(hm2->pins[i]), sizeof(hm2_module_desc_t));
    }
}

hm2_module_desc_t *hm2_find_module(hostmot2_t *hm2, u8 gtag) {
    int i;

    for (i = 0; i < HM2_MAX_MODULES; i++) {
        if (hm2->modules[i].gtag == gtag)
            return &(hm2->modules[i]);
    }
    return NULL;
}

void hm2_set_pin_source(hostmot2_t *hm2, int pin_number, u8 source) {
    u32 data;
    u16 addr;
    hm2_module_desc_t *md = hm2_find_module(hm2, HM2_GTAG_IOPORT);
    
    if (md == NULL) {
        printf("hm2_set_pin_source(): no IOPORT module found\n");
        return;
    }
    if ((pin_number < 0) || (pin_number >= (hm2->idrom.io_ports*hm2->idrom.io_width))) {
        printf("hm2_set_pin_source(): invalid pin number %d\n", pin_number);
        return;
    }

    addr = md->base_address;
    hm2->llio->read(hm2->llio, addr + HM2_MOD_OFFS_GPIO_ALT_SOURCE + (pin_number / 24)*4, &data, sizeof(data));
    if (source == HM2_PIN_SOURCE_IS_PRIMARY) {
        data &= ~(1 << (pin_number % 24));
    } else if (source == HM2_PIN_SOURCE_IS_SECONDARY) {
        data |= (1 << (pin_number % 24));
    } else {
        printf("hm2_set_pin_source(): invalid pin source 0x%02X\n", source);
        return;
    }
    hm2->llio->write(hm2->llio, addr + HM2_MOD_OFFS_GPIO_ALT_SOURCE + (pin_number / 24)*4, &data, sizeof(data));
}

void hm2_set_pin_direction(hostmot2_t *hm2, int pin_number, u8 direction) {
    u32 data;
    u16 addr;
    hm2_module_desc_t *md = hm2_find_module(hm2, HM2_GTAG_IOPORT);

    if (md == NULL) {
        printf("hm2_set_pin_direction(): no IOPORT module found\n");
        return;
    }
    if ((pin_number < 0) || (pin_number >= (hm2->idrom.io_ports*hm2->idrom.io_width))) {
        printf("hm2_set_pin_direction(): invalid pin number %d\n", pin_number);
        return;
    }

    addr = md->base_address;
    hm2->llio->read(hm2->llio, addr + HM2_MOD_OFFS_GPIO_DDR + (pin_number / 24)*4, &data, sizeof(data));
    if (direction == HM2_PIN_DIR_IS_INPUT) {
        data &= ~(1 << (pin_number % 24));
    } else if (direction == HM2_PIN_DIR_IS_OUTPUT) {
        data |= (1 << (pin_number % 24));
    } else {
        printf("hm2_set_pin_direction(): invalid pin direction 0x%02X\n", direction);
        return;
    }
    hm2->llio->write(hm2->llio, addr + HM2_MOD_OFFS_GPIO_DDR + (pin_number / 24)*4, &data, sizeof(data));
}

// PIN FILE GENERATING SUPPORT

static bob_pin_name_t bob_pin_names[MAX_BOB_NAMES] = {
   {BOB_7I76, {"TB2 4,5","TB2 2,3","TB2 10,11","TB2 8,9","TB2 16,17","TB2 14,15","TB2 22,23","TB2 20,21",
    "TB3 4,5","TB3 2,3","Internal","Internal","TB3 18,19","TB3 16,17","TB3 13,14","TB3 10,11","TB3 7,8"}},

   {BOB_7I77, {"Internal","TB6 5,6","TB6 3,4","Internal","Internal","Internal","Internal","Internal","TB3 1,2,9,10","TB3 4,5,12,13",
    "TB3 7,8,15,16","TB3 17,18 TB4 1,2","TB3 20,21 TB4 4,5","TB3 23,24 TB4 7,8","TB4 9,10,17,18","TB4 12,13,20,21","TB4 15,16,23,24"}},

   {BOB_7I94_0, {"P2 1","P2 14","P2 2","P2 15","P2 3","P2 16","P2 4","P2 17","P2 5","P2 6","P2 7",
    "P2 8","P2 9","P2 10","P2 11","P2 12","P2 13","J6 1,2","J6 3,6","J6 TXEN","J7 1,2"}},
   {BOB_7I94_1, {"J7 3,6","J7 TXEN","J8 1,2","J8 3,6","J8 TXEN","J9 1,2","J9 3,6","J9 TXEN","J4 1,2","J4 3,6",
    "J4 TXEN","J3 1,2","J3 3,6","J3 TXEN","J2 1,2","J2 3,6","J2 TXEN","J1 1,2","J1 3,6","J1 TXEN","P2 /ENA"}},
   
   {BOB_7I95_0, {"TB3 2,3","TB3 4,5","TB3 8,9","TB3 10,11","TB3 14,15","TB3 16,17","TB3 20,21","TB3 22,23",
    "TB4 2,3","TB4 4,5","TB4 8,9","TB4 10,11","TB2 14,15","TB4 16,17","Internal","TB4 20,21","TB4 22,23","Internal"
    "TB1 1,2,9,10","TB1 4,5,12,13","TB1 7,8,15,16","TB1 17,18 TB2 1,2","TB1 20,21 TB2 4,5","TB1 23,24 TB2 7,8",
    "TB2 9,10,17,18","TB2 11,12,20,21","TB2 15,16,23,24","Internal","Internal"}},
   {BOB_7I95_1, {"Internal","Internal","Internal","Internal","Internal","TB3 13,14","TB3 15,16","TB3 17,18",
    "TB3 19,20","TB3 21,22","TB3 23,24","Internal","P1 1","P1 14","P1 2","P1 15","P1 3","P1 16","P1 4",
    "P1 17","P1 5","P1 6","P1 7","P1 8","P1 9","P1 10","P1 11","P1 12","P1 13"}},

   {BOB_7I96_0, {"TB3 1","TB3 2","TB3 3","TB3 4","TB3 5","TB3 6","TB3 7","TB3 8","TB3 9","TB3 10","Internal",
    "TB3 13,14","TB3 15,16","TB3 17,18","TB3 19,20","TB3 21,22","TB3 23,24"}},
   {BOB_7I96_1, {"TB1 2,3","TB1 4,5","TB1 8,9","TB1 10,11","TB1 14,15","TB1 16,17","TB1 20,21","TB1 22,23",
    "TB2 2,3","TB2 4,5","TB2 7,8","TB2 10,11","TB2 13,14","TB2 16,17","TB2 18,19","Internal","Internal"}},

   {BOB_7I97_0, {"TB3 4","TB3 8","TB3 12","TB3 16","TB3 20","TB3 20","TB3 24","TB3 24","TB3 4,8,12,16",
    "TB1 1,2,9,10","TB1 4,5,12,13","TB1 7,8,15,16","TB1 17,18 TB2 1,2","TB1 20,21 TB2 4,5","TB1 23,24 TB2 7,8",
    "TB2 9,10,17,18","TB2 12,13,20,21"}},
   {BOB_7I97_1, {"TB2 15,16,23,24","Internal","TB5 13,14","TB5 15,16","TB5 17,18","TB5 19,20","TB5 21,22",
    "TB5 23,24","Internal","Internal","Internal","Internal","Internal","Internal","TB4 15,16","TB4 17,18","Internal"}},

   {BOB_7C80_0, {"TB7 2,3","TB7 4,5","TB8 2,3","TB8 4,5","TB9 2,3","TB9 4,5","TB10 2,3","TB10 4,5","TB11 2,3","TB11 4,5",
    "TB12 2,3","TB13 4,5","TB3 3,4","TB3 5,6","Internal","TB4 1,2","TB4 4,5","TB4 7,8","TB5 2","TB5 2","TB5 5,6","TB5 7,8"
    "Internal","Internal","Internal","Internal","Internal"}},
   {BOB_7C80_1, {"Internal","TB13 1,2","TB13 3,4","TB13 5,6","TB13 7,8","TB14 1,2","TB14 3,4","TB14 5,6","TB14 7,8",
     "Internal", "P1 1","P1 14","P1 2","P1 15","P1 3","P1 16","P1 4","P1 17","P1 5","P1 6","P1 7","P1 8","P1 9","P1 10",
     "P1 11","P1 12","P1 13"}},

   {BOB_7C81_0, {"P1 1","P1 14","P1 2","P1 15","P1 3","P1 16","P1 4","P1 17","P1 5","P1 6","P1 7","P1 8","P1 9","P1 10","P1 11","P1 12","P1 13","P5 3,6","P6 3,6"}},
   {BOB_7C81_1, {"P2 1","P2 14","P2 2","P2 15","P2 3","P2 16","P2 4","P2 17","P2 5","P2 6","P2 7","P2 8","P2 9","P2 10","P2 11","P2 12","P2 13","P5 TXEN","P6 TXEN"}},
   {BOB_7C81_2, {"P7 1","P7 14","P7 2","P7 15","P7 3","P7 16","P7 4","P7 17","P7 5","P7 6","P7 7","P7 8","P7 9","P7 10","P7 11","P7 12","P7 13","P5 1,2","P6 1,2"}}
};

static pin_name_t pin_names[HM2_MAX_TAGS] = {
  {HM2_GTAG_NONE,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_IRQ_LOGIC,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_WATCHDOG,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_IOPORT,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_ENCODER,  {"Quad-A", "Quad-B", "Quad-IDX", "Quad-IDXM", "Quad-Probe", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER,  {"MuxQ-A", "MuxQ-B", "MuxQ-IDX", "MuxQ-IDXM", "Quad-ProbeM", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_SEL,  {"MuxSel0", "MuxSel1", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_MIM,  {"MuxQ-A", "MuxQ-B", "MuxQ-IDX", "MuxQ-IDXM", "Quad-ProbeM", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_SEL_MIM,  {"MuxSel0", "MuxSel1", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_STEPGEN,  {"Step/Table1", "Dir/Table2", "Table3", "Table4", "Table5", "Table6", "SGindex", "SGProbe", "Null9", "Null10"}},
  {HM2_GTAG_PWMGEN,      {"PWM", "Dir", "/Enable", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_RCPWMGEN,      {"PWM", "Null2","Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TPPWM,    {"PWMA", "PWMB", "PWMC", "NPWMA", "NPWMB", "NPWMC", "/ENA", "FAULT", "Null9", "Null10"}},
  {HM2_GTAG_WAVEGEN,  {"PDMA", "PDMB", "Trig0", "Trig1", "Trig2", "Trig3", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_DAQ_FIFO,  {"Data", "Strobe", "Full", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BIN_OSC,  {"OscOut", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BIN_DMDMA,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_RESOLVER,  {"PwrEn", "PDMP", "PDMM", "ADChan0", "ADChan1", "ADChan2", "SPICS", "SPIClk", "SPIDI0", "SPIDI1"}},
  {HM2_GTAG_SSERIAL,  {"RXData", "TXData", "TXEn", "TestPin", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TWIDDLER,  {"InBit", "IOBit", "OutBit", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SSR,       {"Out", "AC Ref", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SPI,      {"/Frame", "DOut", "SClk", "DIn", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BSPI,     {"/Frame", "DOut", "SClk", "DIn", "CS0", "CS1", "CS2", "CS3", "Null9", "Null10"}},
  {HM2_GTAG_DBSPI,    {"Null1", "DOut", "SClk", "DIn", "/CS-FRM0", "/CS-FRM1", "/CS-FRM2", "/CS-FRM3", "Null9", "Null10"}},
  {HM2_GTAG_DPLL,     {"Sync", "DDSMSB", "FOut", "PostOut", "SyncToggle", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SSI,      {"SClk", "SClkEn", "Data", "DAv", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BISS,      {"Clk", "ClkEn", "Din", "DAv", "TData", "STime", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_UART_TX,   {"TXData", "TXEna", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_UART_RX,   {"RXData", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TRAM,      {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_LED,       {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_INMUX,     {"Addr0", "Addr1", "Addr2", "Addr3", "Addr4", "MuxData", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SIGMA5,    {"TXData", "RXData", "TxEn", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_INM,       {"Input0", "Input1", "Input2", "Input3", "Input4", "Input5", "Input6", "Input7", "Input8"}}, 
  {HM2_GTAG_XYMOD,     {"XData", "YData", "Clk", "Sync", "Status", "Null6", "Null7", "Null8", "Null9", "Null10"}},
};

static pin_name_t pin_names_xml[HM2_MAX_TAGS] = {
  {HM2_GTAG_NONE,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_IRQ_LOGIC,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_WATCHDOG,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_IOPORT,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_ENCODER,  {"Phase A", "Phase B", "Index", "Quad-IDXM", "Quad-Probe", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER,  {"Muxed Phase A", "Muxed Phase B", "Muxed Index", "MuxQ-IDXM", "Quad-ProbeM", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_SEL,  {"Muxed Encoder Select 0", "Muxed Encoder select 1", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_MIM,  {"MuxQ-A", "MuxQ-B", "MuxQ-IDX", "MuxQ-IDXM", "Quad-ProbeM", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_MUXED_ENCODER_SEL_MIM,  {"MuxSel0", "MuxSel1", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_STEPGEN,  {"Step/Table1", "Dir/Table2", "Table3", "Table4", "Table5", "Table6", "SGindex", "SGProbe", "Null9", "Null10"}},
  {HM2_GTAG_PWMGEN,      {"PWM/Up", "Dir/Down", "/Enable", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_RCPWMGEN,      {"PWM", "Null2","Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TPPWM,    {"PWMA", "PWMB", "PWMC", "NPWMA", "NPWMB", "NPWMC", "/ENA", "FAULT", "Null9", "Null10"}},
  {HM2_GTAG_WAVEGEN,  {"PDMA", "PDMB", "Trig0", "Trig1", "Trig2", "Trig3", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_DAQ_FIFO,  {"Data", "Strobe", "Full", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BIN_OSC,  {"OscOut", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BIN_DMDMA,  {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_RESOLVER,  {"PwrEn", "PDMP", "PDMM", "ADChan0", "ADChan1", "ADChan2", "SPICS", "SPIClk", "SPIDI0", "SPIDI1"}},
  {HM2_GTAG_SSERIAL,  {"RXData", "TXData", "TXEn", "TestPin", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TWIDDLER,  {"InBit", "IOBit", "OutBit", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SSR,       {"Out", "AC Ref", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SPI,      {"/Frame", "DOut", "SClk", "DIn", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BSPI,     {"/Frame", "DOut", "SClk", "DIn", "CS0", "CS1", "CS2", "CS3", "Null9", "Null10"}},
  {HM2_GTAG_DBSPI,    {"Null1", "DOut", "SClk", "DIn", "/CS-FRM0", "/CS-FRM1", "/CS-FRM2", "/CS-FRM3", "Null9", "Null10"}},
  {HM2_GTAG_DPLL,     {"Sync", "DDSMSB", "FOut", "PostOut", "SyncToggle", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SSI,      {"SClk", "SClkEn", "Din", "DAv", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_BISS,      {"Clk", "ClkEn", "Din", "DAv", "TData", "STime", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_UART_TX,   {"TXData", "TXEna", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_UART_RX,   {"RXData", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_TRAM,    {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_LED,      {"Null1", "Null2", "Null3", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_INMUX, {"Addr0", "Addr1", "Addr2", "Addr3", "Addr4", "MuxData", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_SIGMA5, {"TXData", "RXData", "TxEn", "Null4", "Null5", "Null6", "Null7", "Null8", "Null9", "Null10"}},
  {HM2_GTAG_INM,       {"Input0", "Input1", "Input2", "Input3", "Input4", "Input5", "Input6", "Input7", "Input8"}}, 
  {HM2_GTAG_XYMOD, {"XData", "YData", "Clk", "Sync", "Status", "Null6", "Null7", "Null8", "Null9", "Null10"}},
};

static mod_name_t mod_names[HM2_MAX_TAGS] = {
    {"Null",        HM2_GTAG_NONE},
    {"IRQLogic",    HM2_GTAG_IRQ_LOGIC},
    {"WatchDog",    HM2_GTAG_WATCHDOG},
    {"IOPort",      HM2_GTAG_IOPORT},
    {"QCount",      HM2_GTAG_ENCODER},
    {"StepGen",     HM2_GTAG_STEPGEN},
    {"PWM",         HM2_GTAG_PWMGEN},
    {"RCPWM",       HM2_GTAG_RCPWMGEN},
    {"SPI",         HM2_GTAG_SPI},
    {"SSI",         HM2_GTAG_SSI},
    {"UARTTX",      HM2_GTAG_UART_TX},
    {"UARTRX",      HM2_GTAG_UART_RX},
    {"AddrX",       HM2_GTAG_TRAM},
    {"MuxedQCount", HM2_GTAG_MUXED_ENCODER},
    {"MuxedQCountSel", HM2_GTAG_MUXED_ENCODER_SEL},
    {"BufSPI",      HM2_GTAG_BSPI},
    {"DBufSPI",     HM2_GTAG_DBSPI},
    {"DPLL",        HM2_GTAG_DPLL},
    {"MuxQCntM",    HM2_GTAG_MUXED_ENCODER_MIM},
    {"MuxQSelM",    HM2_GTAG_MUXED_ENCODER_SEL_MIM},
    {"TPPWM",       HM2_GTAG_TPPWM},
    {"WaveGen",     HM2_GTAG_WAVEGEN},
    {"DAQFIFO",     HM2_GTAG_DAQ_FIFO},
    {"BinOsc",      HM2_GTAG_BIN_OSC},
    {"DMDMA",       HM2_GTAG_BIN_DMDMA},
    {"DPLL",        HM2_GTAG_HM2DPLL},
    {"LED",         HM2_GTAG_LED},
    {"ResolverMod", HM2_GTAG_RESOLVER},
    {"SSerial",     HM2_GTAG_SSERIAL},
    {"Twiddler",    HM2_GTAG_TWIDDLER},
    {"SSR",         HM2_GTAG_SSR},
    {"InMux",       HM2_GTAG_INMUX},
    {"Sigma5Enc",   HM2_GTAG_SIGMA5},
    {"InM",         HM2_GTAG_INM},
    {"BISS",        HM2_GTAG_BISS},
    {"XYMod",       HM2_GTAG_XYMOD},
};

static mod_name_t mod_names_xml[HM2_MAX_TAGS] = {
    {"Null",        HM2_GTAG_NONE},
    {"IRQLogic",    HM2_GTAG_IRQ_LOGIC},
    {"Watchdog",    HM2_GTAG_WATCHDOG},
    {"IOPort",      HM2_GTAG_IOPORT},
    {"Encoder",     HM2_GTAG_ENCODER},
    {"StepGen",     HM2_GTAG_STEPGEN},
    {"PWM",         HM2_GTAG_PWMGEN},
    {"RCPWM",       HM2_GTAG_RCPWMGEN},
    {"SPI",         HM2_GTAG_SPI},
    {"SSI",         HM2_GTAG_SSI},
    {"UARTTX",      HM2_GTAG_UART_TX},
    {"UARTRX",      HM2_GTAG_UART_RX},
    {"AddrX",       HM2_GTAG_TRAM},
    {"MuxedQCount", HM2_GTAG_MUXED_ENCODER},
    {"MuxedQCountSel", HM2_GTAG_MUXED_ENCODER_SEL},
    {"BufSPI",      HM2_GTAG_BSPI},
    {"DBufSPI",     HM2_GTAG_DBSPI},
    {"DPLL",        HM2_GTAG_DPLL},
    {"MuxQCntM",    HM2_GTAG_MUXED_ENCODER_MIM},
    {"MuxQSelM",    HM2_GTAG_MUXED_ENCODER_SEL_MIM},
    {"TPPWM",       HM2_GTAG_TPPWM},
    {"WaveGen",     HM2_GTAG_WAVEGEN},
    {"DAQFIFO",     HM2_GTAG_DAQ_FIFO},
    {"BinOsc",      HM2_GTAG_BIN_OSC},
    {"DMDMA",       HM2_GTAG_BIN_DMDMA},
    {"DPLL",        HM2_GTAG_HM2DPLL},
    {"LED",         HM2_GTAG_LED},
    {"ResolverMod", HM2_GTAG_RESOLVER},
    {"SSerial",     HM2_GTAG_SSERIAL},
    {"Twiddler",    HM2_GTAG_TWIDDLER},
    {"SSR",         HM2_GTAG_SSR},
    {"InMux",       HM2_GTAG_INMUX},
    {"Sigma5Enc",   HM2_GTAG_SIGMA5},
    {"InM",         HM2_GTAG_INM},
    {"BISS",        HM2_GTAG_BISS},
    {"XYMod",       HM2_GTAG_XYMOD},
};

static char *find_module_name(int gtag, int xml_flag) {
    int i;
    static char unknown[100];
    mod_name_t *mod_names_ptr;

    if (gtag == HM2_GTAG_NONE) {
        return "None";
    }

    if (xml_flag == 0) {
        mod_names_ptr = mod_names;
    } else {
        mod_names_ptr = mod_names_xml;
    }

    for (i = 0; i < HM2_MAX_TAGS; i++) {
        if (mod_names_ptr[i].tag == gtag)
            return mod_names_ptr[i].name;
    }

    snprintf(unknown, 100, "(unknown-gtag-%d)", gtag);
    return unknown;
}


static char *pin_get_pin_name(hm2_pin_desc_t *pin, int xml_flag) {
    int i;
    u8 chan;
    static char buff[100];
    pin_name_t *pin_names_ptr;
    
    if (xml_flag == 0) {
        pin_names_ptr = pin_names;
    } else {
        pin_names_ptr = pin_names_xml;
    }

    for (i = 0; i < HM2_MAX_TAGS; i++) {
        if (pin_names_ptr[i].tag == pin->sec_tag) {
            if (pin->sec_tag == HM2_GTAG_SSERIAL) {
                chan = (pin->sec_pin & 0x0F) - 1;
                if ((pin->sec_pin & 0xF0) == 0x00) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[0], chan);
                    return buff;
                } else if ((pin->sec_pin & 0xF0) == 0x80) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[1], chan);
                    return buff;
                } else if ((pin->sec_pin & 0xF0) == 0x90) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[2], chan);
                    return buff;
                } else if (pin->sec_pin == 0xA1) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[3], chan);
                    return buff;
                }
            } else if (pin->sec_tag == HM2_GTAG_SSR) {
                if ((pin->sec_pin & 0xF0) == 0x80) {
                    chan = pin->sec_pin - 0x81;
                    sprintf(buff, "%s-%02u", pin_names_ptr[i].name[0], chan);
                    return buff;
                } else if (pin->sec_pin == 0xA0) {
                    sprintf(buff, "%s", pin_names_ptr[i].name[1]);
                    return buff;
                }
            } else if (pin->gtag == HM2_GTAG_DAQ_FIFO) {
                chan = pin->sec_chan & 0x1F;
                if ((pin->sec_pin & 0xE0) == 0x00) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[0], chan);
                    return buff;
                } else if (pin->sec_pin == 0x41) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[1], chan);
                    return buff;
                } else if (pin->sec_pin == 0x81) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[2], chan);
                    return buff;
                }
            } else if (pin->gtag == HM2_GTAG_TWIDDLER) {
                chan = pin->sec_chan & 0x1F;
                if ((pin->sec_pin & 0xC0) == 0x00) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[0], chan);
                    return buff;
                } else if ((pin->sec_pin & 0xC0) == 0xC0) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[1], chan);
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[1], chan);
                    return buff;
                } else if ((pin->sec_pin & 0xC0) == 0x80) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[2], chan);
                    return buff;
                }
            } else if (pin->gtag == HM2_GTAG_BIN_OSC) {
                chan = pin->sec_chan & 0x1F;
                if ((pin->sec_pin & 0x80) == 0x80) {
                    sprintf(buff, "%s%u", pin_names_ptr[i].name[0], chan);
                    return buff;
                }
            } else if (pin->sec_tag == HM2_GTAG_INMUX) {
                if ((pin->sec_pin & 0x80) == 0x80) {
                    // output pins
                    snprintf(buff, sizeof(buff), "Addr%d", pin->sec_pin - 0x81);
                } else {
                    // input pins
                    snprintf(buff, sizeof(buff), "Data%d", pin->sec_pin - 0x01);
                }
                return buff;
            } else if (pin->sec_tag == HM2_GTAG_INM) {
                snprintf(buff, sizeof(buff), "Input%d", pin->sec_pin - 0x01);
                return buff;
            } else {
                sprintf(buff, "%s", pin_names_ptr[i].name[(pin->sec_pin & 0x0F) - 1]);
                return buff;
            }
        };
    };
    snprintf(buff, sizeof(buff), "Unknown-Gtag%d-Chan%d-Pin%d", pin->sec_tag, pin->sec_chan, pin->sec_pin);
    return buff;
}

void hm2_print_pin_file(llio_t *llio, int xml_flag) {
    unsigned int i, j;
    const u8 db25_pins[] = {1, 14, 2, 15, 3, 16, 4, 17, 5, 6, 7, 8, 9, 10, 11, 12, 13};

    if (xml_flag == 0) {
        printf("Configuration Name: %.*s\n", 8, llio->hm2.config_name);
        printf("\nGeneral configuration information:\n\n");
        printf("  BoardName : %.*s\n", (int)sizeof(llio->hm2.idrom.board_name), llio->hm2.idrom.board_name);
        printf("  FPGA Size: %u KGates\n", llio->hm2.idrom.fpga_size);
        printf("  FPGA Pins: %u\n", llio->hm2.idrom.fpga_pins);
        printf("  Number of IO Ports: %u\n", llio->hm2.idrom.io_ports);
        printf("  Width of one I/O port: %u\n", llio->hm2.idrom.port_width);
        printf("  Clock Low frequency: %.4f MHz\n", (float) llio->hm2.idrom.clock_low/1000000.0);
        printf("  Clock High frequency: %.4f MHz\n", (float) llio->hm2.idrom.clock_high/1000000.0);
        printf("  IDROM Type: %u\n", llio->hm2.idrom.idrom_type);
        printf("  Instance Stride 0: %u\n", llio->hm2.idrom.instance_stride0);
        printf("  Instance Stride 1: %u\n", llio->hm2.idrom.instance_stride1);
        printf("  Register Stride 0: %u\n", llio->hm2.idrom.register_stride0);
        printf("  Register Stride 1: %u\n", llio->hm2.idrom.register_stride1);

        printf("\nModules in configuration:\n\n");
        for (i = 0; i < HM2_MAX_MODULES; i++) {
            if ((llio->hm2.modules[i].gtag == 0) && (llio->hm2.modules[i].version == 0) &&
            (llio->hm2.modules[i].clock_tag == 0) && (llio->hm2.modules[i].instances == 0)) break;

            printf("  Module: %s\n", find_module_name(llio->hm2.modules[i].gtag, xml_flag));
            printf("  There are %u of %s in configuration\n", llio->hm2.modules[i].instances, find_module_name(llio->hm2.modules[i].gtag, xml_flag));

            printf("  Version: %u\n", llio->hm2.modules[i].version);
            printf("  Registers: %u\n", llio->hm2.modules[i].registers);
            printf("  BaseAddress: %04X\n", llio->hm2.modules[i].base_address);
            printf("  ClockFrequency: %.3f MHz\n",
              llio->hm2.modules[i].clock_tag == HM2_CLOCK_LOW_TAG ? (float) llio->hm2.idrom.clock_low/1000000.0 : (float) llio->hm2.idrom.clock_high/1000000.0);
            printf("  Register Stride: %u bytes\n",
              (llio->hm2.modules[i].strides & 0x0F) == 0 ? llio->hm2.idrom.register_stride0 : llio->hm2.idrom.register_stride1);
            printf("  Instance Stride: %u bytes\n\n",
              (llio->hm2.modules[i].strides & 0xF0) == 0 ? llio->hm2.idrom.instance_stride0 : llio->hm2.idrom.instance_stride1);
        }

        printf("Configuration pin-out:\n");
        for (i = 0; i < llio->hm2.idrom.io_ports; i++) {
            printf("\nIO Connections for %s\n", llio->ioport_connector_name[i]);
            printf("Pin#                  I/O   Pri. func    Sec. func       Chan      Pin func        Pin Dir\n\n");
            for (j = 0; j < llio->hm2.idrom.port_width; j++) {
                hm2_pin_desc_t *pin = &(llio->hm2.pins[i*(llio->hm2.idrom.port_width) + j]);
                int pin_nr;

                switch (llio->hm2.idrom.port_width) {
                    case 17:
                        pin_nr = db25_pins[(i*(llio->hm2.idrom.port_width) + j) % 17];
                        break;
                    case 24:
                        pin_nr = (i*(llio->hm2.idrom.port_width) + j) % (llio->hm2.idrom.port_width)*2 + 1;
                        break;
                    case 32:
                        pin_nr = i*(llio->hm2.idrom.port_width) + j;
                        break;
                    default:
			pin_nr = 0;
			break;
                }
                if (llio->bob_hint[i] != 0) {
                    printf("%-18s",bob_pin_names[llio->bob_hint[i]-1].name[j]); 
		} else {
                    printf("%2u                ", pin_nr);
                }
                printf("    %3u", i*(llio->hm2.idrom.port_width) + j);
                printf("   %-8s", find_module_name(pin->gtag, xml_flag));
                if (pin->sec_tag == HM2_GTAG_NONE) {
                    printf("     %-15s", "None");
                } else {
                    printf("     %-15s", find_module_name(pin->sec_tag, xml_flag));

                    if (pin->sec_chan & HM2_CHAN_GLOBAL) {
                        printf("    Global    ");
                    } else {
                        printf(" %2u        ", pin->sec_chan);
                    }
                    printf("%-12s", pin_get_pin_name(pin, xml_flag));

                    if (pin->sec_pin & HM2_PIN_OUTPUT) {
                        printf("    (Out)");
                    } else {
                        printf("    (In)");
                    }
                }

                printf("\n");
            }
        }
        printf("\n");
    } else {
        printf("<?xml version=\"1.0\"?>\n");
        printf("<hostmot2>\n");
        printf("    <boardname>%.*s</boardname>\n", (int)sizeof(llio->hm2.idrom.board_name), llio->hm2.idrom.board_name);
        printf("    <ioports>%d</ioports>\n", llio->hm2.idrom.io_ports);
        printf("    <iowidth>%d</iowidth>\n", llio->hm2.idrom.port_width*llio->hm2.idrom.io_ports);
        printf("    <portwidth>%d</portwidth>\n", llio->hm2.idrom.port_width);
        printf("    <clocklow>%8d</clocklow>\n", llio->hm2.idrom.clock_low);
        printf("    <clockhigh>%8d</clockhigh>\n", llio->hm2.idrom.clock_high);

        printf("    <modules>\n");
        for (i = 0; i < HM2_MAX_MODULES; i++) {
            if ((llio->hm2.modules[i].gtag == 0) && (llio->hm2.modules[i].version == 0) &&
            (llio->hm2.modules[i].clock_tag == 0) && (llio->hm2.modules[i].instances == 0)) break;

            printf("        <module>\n");
            printf("            <tagname>%s</tagname>\n", find_module_name(llio->hm2.modules[i].gtag, xml_flag));
            printf("            <numinstances>%2d</numinstances>\n", llio->hm2.modules[i].instances);
            printf("        </module>\n");
        }
        printf("    </modules>\n");

        printf("    <pins>\n");
        for (i = 0; i < llio->hm2.idrom.io_ports; i++) {
            for (j = 0; j < llio->hm2.idrom.port_width; j++) {
                hm2_pin_desc_t *pin = &(llio->hm2.pins[i*(llio->hm2.idrom.port_width) + j]);
                int pin_nr;

                switch (llio->hm2.idrom.port_width) {
                    case 17:
                        pin_nr = db25_pins[(i*(llio->hm2.idrom.port_width) + j) % 17];
                        break;
                    case 24:
                        pin_nr = (i*(llio->hm2.idrom.port_width) + j) % (llio->hm2.idrom.port_width)*2 + 1;
                        break;
                    case 32:
                        pin_nr = i*(llio->hm2.idrom.port_width) + j;
                        break;
                    default:
			pin_nr = 0;
                        break;
                }
                printf("        <pin>\n");
                printf("            <connector>%s</connector>\n", llio->ioport_connector_name[i]);
                printf("            <secondarymodulename>%s</secondarymodulename>\n", find_module_name(pin->sec_tag, xml_flag));
                printf("            <secondaryfunctionname>");
                if (pin->sec_tag != HM2_GTAG_NONE) {
                    printf("%s", pin_get_pin_name(pin, xml_flag));
                    if (pin->sec_pin & HM2_PIN_OUTPUT) {
                        printf(" (Out)");
                    } else {
                        printf(" (In)");
                    }
                }
                printf("</secondaryfunctionname>\n");               
                if (llio->bob_hint[i] != 0) {
                    printf("            <secondaryinstance>%-18s</secondaryinstance>\n",bob_pin_names[llio->bob_hint[i]-1].name[j]); 
                } else {
                    printf("            <secondaryinstance>%2d</secondaryinstance>\n", pin_nr);
                }
                printf("        </pin>\n");
            }
        }
        printf("    </pins>\n");
        printf("</hostmot2>\n");
    }
}

void hm2_print_pin_descriptors(llio_t *llio) {
    int num_pins;

    num_pins = llio->hm2.idrom.io_ports * llio->hm2.idrom.port_width;
    printf("%d HM2 Pin Descriptors:\n", num_pins);

    for (int i = 0; i < num_pins; i ++) {
        hm2_pin_desc_t *pd = &llio->hm2.pins[i];

        printf("    pin %d:\n", i);
        printf(
            "        Primary Tag: 0x%02X (%s)\n",
            pd->gtag,
            find_module_name(pd->gtag, 0)
        );
        if (llio->hm2.pins[i].sec_tag != 0) {
            printf(
                "        Secondary Tag: 0x%02X (%s)\n",
                llio->hm2.pins[i].sec_tag,
                find_module_name(pd->sec_tag, 0)
            );
            printf("        Secondary Unit: 0x%02X\n", pd->sec_chan);
            printf(
                "        Secondary Pin: 0x%02X (%s, %s)\n",
                pd->sec_pin,
                pin_get_pin_name(pd, 0),
                (pd->sec_pin & 0x80) ? "Output" : "Input"
            );
        }
    }
}
