/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2021 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


#include "configure.h"

#if WITH_ADC

#include "adc.h"



#if ADC_PERIOD > 0
static uint16_t tNextSample;
static uint8_t sampling;    // 0 = not sampling, 1 = sampling for ADC #0, 2 = sampling for ADC #1
#endif





// *************************** Configuration ***********************************


#if MCU_TYPE == BR_MCU_ATTINY85

#define ADC_MUX_OF_PORT(P) \
  ((P) == P_B5 ? 0 : (P) == P_B2 ? 1 : (P) == P_B4 ? 2 : (P) == P_B3 ? 3 :    \
   0)

#elif MCU_TYPE == BR_MCU_ATTINY84

#define ADC_MUX_OF_PORT(P) \
  ((P) == P_A0 ? 0 : (P) == P_A1 ? 1 : (P) == P_A2 ? 2 : (P) == P_A3 ? 3 :    \
   (P) == P_A4 ? 4 : (P) == P_A5 ? 5 : (P) == P_A6 ? 6 : (P) == P_A7 ? 7 :    \
   0)

#elif MCU_TYPE == BR_MCU_ATTINY861

#define ADC_MUX_OF_PORT(P) \
  ((P) == P_A0 ? 0 : (P) == P_A1 ? 1 : (P) == P_A2 ? 2 : (P) == P_A4 ? 3 :    \
   (P) == P_A5 ? 4 : (P) == P_A6 ? 5 : (P) == P_A7 ? 6 : (P) == P_B4 ? 7 :    \
   (P) == P_B5 ? 8 : (P) == P_B6 ? 9 : (P) == P_B7 ? 10 :  \
   0)

#else
#error "Unsupported MCU"
#endif

#define ADC_0_MUX (ADC_MUX_OF_PORT(P_ADC_0))
#define ADC_1_MUX (ADC_MUX_OF_PORT(P_ADC_1))

#if (ADC_PORTS >= 1) && !ADC_0_MUX
#error "Undefined or illegal port for ADC #0"
#endif

#if (ADC_PORTS >= 2) && !ADC_1_MUX
#error "Undefined or illegal port for ADC #1"
#endif



// ***** Sanity checks *****


#if (P_ADC_0_STROBE & ~(GPIO_TO_PMASK (~0))) != 0
#error "Illegal value for P_ADC_0_STROBE: Only GPIO-capable pins allowed"
#endif

#if (P_ADC_0_STROBE & (P_ADC_0 | (ADC_PORTS >= 2 ? P_ADC_1 : 0) | P_ADC_1_STROBE))
#error "Illegal value for P_ADC_0_STROBE: Confict with some other ADC pin"
#endif

#if (P_ADC_1_STROBE & ~(GPIO_TO_PMASK (~0))) != 0
#error "Illegal value for P_ADC_1_STROBE: Only GPIO-capable pins allowed"
#endif

#if (P_ADC_1_STROBE & (P_ADC_0 | (ADC_PORTS >= 2 ? P_ADC_1 : 0) | P_ADC_0_STROBE))
#error "Illegal value for P_ADC_1_STROBE: Confict with some other ADC pin"
#endif




// *************************** Helpers *****************************************


static inline void AdcStartSampling (uint8_t mux40) {
  ADMUX = (1 << ADLAR) | mux40;   // set mux
  ADCSRA |= (1 << ADSC);          // start conversion
}


#define ADC_IS_SAMPLING (ADCSRA & (1 << ADSC))


static inline int16_t AdcSetStrobes (int16_t tLeft) {
#if P_ADC_0_STROBE
  static bool adc0strobe;
  if (tLeft > ADC_0_STROBE_TICKS) {
    // Reset strobe output ...
    if (ADC_0_STROBE_VALUE) P_OUT_0 (P_ADC_0_STROBE);
    else P_OUT_1 (P_ADC_0_STROBE);
    adc0strobe = false;
  }
  else {
    // Set strobe output ...
    if (ADC_0_STROBE_VALUE) P_OUT_1 (P_ADC_0_STROBE);
    else P_OUT_0 (P_ADC_0_STROBE);
    if (!adc0strobe) {
      tLeft = ADC_0_STROBE_TICKS;   // make sure that the strobe is long enough
      adc0strobe = true;
    }
  }
#endif // P_ADC_0_STROBE

#if P_ADC_1_STROBE
  static bool adc1strobe;
  if (tLeft >= ADC_1_STROBE_TICKS) {
    // Reset strobe output ...
    if (ADC_1_STROBE_VALUE) P_OUT_0 (P_ADC_1_STROBE);
    else P_OUT_1 (P_ADC_1_STROBE);
    adc1strobe = false;
  }
  else {
    // Set strobe output ...
    if (ADC_1_STROBE_VALUE) P_OUT_1 (P_ADC_1_STROBE);
    else P_OUT_0 (P_ADC_1_STROBE);
    if (!adc1strobe) {
      tLeft = ADC_1_STROBE_TICKS;   // make sure that the strobe is long enough
      adc1strobe = true;
    }
  }
#endif // P_ADC_1_STROBE

  return tLeft;
}





// *************************** AdcInit() ***************************************


#define DID_MASK ((P_ADC_0 ? (1 << ADC_MUX_OF_PORT (P_ADC_0)) : 0) | (P_ADC_1 ? (1 << ADC_MUX_OF_PORT (P_ADC_1)) : 0))
  // raw mask for the DIDRx registers (digital input disable)


void AdcInit () {
  DIDR0 = (DID_MASK & 0x07) | ((DID_MASK << 1) & 0xf0);    // digital input disable ...
  DIDR1 = (DID_MASK >> 3) & 0xf0;
  ADCSRB = 0;     // ADC Control and Status Register B: Set all defaults
  ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
    // ADC Control and Status Register B: enable ADC, set prescaler to 8 (125 kHz with 1 MHz main clock)
#if ADC_PERIOD > 0
  tNextSample = TimerNow () + ADC_PERIOD;
#endif
}





// *************************** AdcIterate() ************************************


#if ADC_PERIOD > 0    // Iteration updates are only done in active mode.


void AdcIterate () {
  int16_t tLeft;
  uint8_t valLo, valHi;

  switch (sampling) {
    case 0:     // Not sampling ...
      tLeft = tNextSample - TimerNow ();
      tLeft = AdcSetStrobes (tLeft);
      if (tLeft < 0) {

        // Start sampling ADC #0 ...
        AdcStartSampling (ADC_MUX_OF_PORT (P_ADC_0));
        sampling = 1;
        tNextSample = TimerNow () + ADC_PERIOD;
      }
      break;

    case 1:     // Sampling for ADC #0 ...
      if (!ADC_IS_SAMPLING) {
        valLo = ADCL;
        valHi = ADCH;
        if (RegGet (BR_REG_ADC_0_LO) != valLo || RegGet (BR_REG_ADC_0_LO) != valHi) {
          RegSet (BR_REG_ADC_0_LO, valLo);
          RegSet (BR_REG_ADC_0_HI, valHi);
          ReportChange (BR_CHANGED_ADC);
        }
        if (P_ADC_1) {
          AdcStartSampling (ADC_MUX_OF_PORT (P_ADC_1));
          sampling = 2;
        }
        else sampling = 0;
      }
      break;

    case 2:     // Sampling for ADC #1 ...
    default:
      if (!ADC_IS_SAMPLING) {
        valLo = ADCL;
        valHi = ADCH;
        if (RegGet (BR_REG_ADC_1_LO) != valLo || RegGet (BR_REG_ADC_1_LO) != valHi) {
          RegSet (BR_REG_ADC_1_LO, valLo);
          RegSet (BR_REG_ADC_1_HI, valHi);
          ReportChange (BR_CHANGED_ADC);
        }
        sampling = 0;
      }
      break;
  }
}


#endif // ADC_PERIOD > 0





// *************************** AdcOnRegRead() **********************************


#if ADC_PERIOD == 0    // Iteration updates are only done in passive (on-demand) mode.


#define ADC_DO_SAMPLE(REG_LO, REG_HI, P_ADC, P_STROBE, STROBE_TICKS, STROBE_VALUE) {  \
                                                                                      \
  /* Activate strobe output ... */                                                    \
  if (P_STROBE) {                                                                     \
    if (ADC_0_STROBE_VALUE) P_OUT_1 (P_STROBE);                                       \
    else P_OUT_0 (P_STROBE);                                                          \
  }                                                                                   \
                                                                                      \
  /* Wait for strobe time ... */                                                      \
  tSample = TimerNow () + STROBE_TICKS;                                               \
  while (TimerNow () < tSample) {}                                                    \
                                                                                      \
  /* Sample ... */                                                                    \
  AdcStartSampling (ADC_MUX_OF_PORT (P_ADC));                                         \
  while (ADC_IS_SAMPLING) {}                                                          \
  RegSet (REG_LO, ADCL);                                                              \
  RegSet (REG_HI, ADCH);                                                              \
                                                                                      \
  /* Reset strobe output ... */                                                       \
  if (P_STROBE) {                                                                     \
    if (STROBE_VALUE) P_OUT_0 (P_STROBE);                                             \
    else P_OUT_1 (P_STROBE);                                                          \
  }                                                                                   \
}


void AdcOnRegRead (uint8_t reg) {
  uint16_t tSample;

  // ADC #0 ...
  static bool adc0latched = false;
  if (reg == BR_REG_ADC_0_LO || reg == BR_REG_ADC_0_HI) {
    // Strobe and sample if a) the low register is accessed or b) the previous access was to the high register ...
    if (reg == BR_REG_ADC_0_LO || !adc0latched)
      ADC_DO_SAMPLE(BR_REG_ADC_0_LO, BR_REG_ADC_0_HI, P_ADC_0, P_ADC_0_STROBE, ADC_0_STROBE_TICKS, ADC_0_STROBE_VALUE);
    adc0latched = (reg == BR_REG_ADC_0_LO);
  }

#if ADC_PORTS > 1

  // ADC #1 ...
  static bool adc1latched = false;
  if (reg == BR_REG_ADC_1_LO || reg == BR_REG_ADC_1_HI) {
    // Strobe and sample if a) the low register is accessed or b) the previous access was to the high register ...
    if (reg == BR_REG_ADC_1_LO || !adc1latched)
      ADC_DO_SAMPLE(BR_REG_ADC_1_LO, BR_REG_ADC_1_HI, P_ADC_1, P_ADC_1_STROBE, ADC_1_STROBE_TICKS, ADC_1_STROBE_VALUE);
    adc1latched = (reg == BR_REG_ADC_1_LO);
  }

#endif // ADC_PORTS > 1

}


#endif // ADC_PERIOD > 0





#endif // WITH_ADC
