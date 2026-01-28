#include "Interrupts.h"
#include <iostream>

bool Interrupt::isKthBitSet(int n, int k)
{
    return (n & (1 << k));
}

void Interrupt::requestInterrupt(MMU* mmu, uint8_t id) {
    uint8_t IF_register = mmu->read8(0xFF0F); // Get value of IF Register
    IF_register |= (1 << id); // Set bit for the interruption
    mmu->write8(0xFF0F, IF_register); // Update IF Register
}

bool Interrupt::checkForInterrupts(MMU* mmu, bool* isHaltedInterr, bool* IMEInter, uint16_t* pcInterr) {
    uint8_t if_reg = mmu->read8(0xFF0F); // Interrupt Flag (Solicitadas)
    uint8_t ie_reg = mmu->read8(0xFFFF); // Interrupt Enable (Habilitadas)

    // Solo nos interesan las interrupciones que están Solicitadas Y Habilitadas a la vez
    uint8_t interrupts = if_reg & ie_reg;

    // 1. MANEJO DEL HALT (IMPORTANTE)
    // Si hay alguna interrupción pendiente, la CPU debe despertar del HALT,
    // independientemente de si el IME está activado o no.
    if (interrupts > 0) {
        if (*isHaltedInterr) {
            *isHaltedInterr = false;
        }
    }

    // 2. SI EL MASTER SWITCH (IME) ESTÁ APAGADO, NO PROCESAMOS NADA MÁS
    // (La CPU despertó, pero no salta a la rutina de interrupción)
    if (!(*IMEInter)) {
        return false;
    }

    // 3. PROCESAR INTERRUPCIONES POR PRIORIDAD
    // Si hay interrupciones pendientes y el IME está ON, procesamos SOLO UNA.
    // El orden de prioridad en GameBoy es: 0 (VBlank) > 1 (LCD) > 2 (Timer) > 3 (Serial) > 4 (Joypad)

    if (interrupts > 0) {

        // V-Blank (Bit 0) - Prioridad Máxima
        if (isKthBitSet(interrupts, 0)) {
            doInterrupt(mmu, 0, isHaltedInterr, IMEInter, pcInterr);
            return true; // RETORNAR: Solo atendemos una interrupción por ciclo step()
        }

        // LCD STAT (Bit 1)
        if (isKthBitSet(interrupts, 1)) {
            doInterrupt(mmu, 1, isHaltedInterr, IMEInter, pcInterr);
            return true;
        }

        // Timer (Bit 2)
        if (isKthBitSet(interrupts, 2)) {
            doInterrupt(mmu, 2, isHaltedInterr, IMEInter, pcInterr);
            return true;
        }

        // Serial (Bit 3)
        if (isKthBitSet(interrupts, 3)) {
            doInterrupt(mmu, 3, isHaltedInterr, IMEInter, pcInterr);
            return true;
        }

        // Joypad (Bit 4) - Prioridad Mínima
        if (isKthBitSet(interrupts, 4)) {
            doInterrupt(mmu, 4, isHaltedInterr, IMEInter, pcInterr);
            return true;
        }
    }

    return false;
}

void Interrupt::doInterrupt(MMU* mmu, uint8_t bitToSearch, bool* isHaltedInterr, bool* IMEInter, uint16_t* pcInterr) {
    // Apagar el Interrupt Master Enable
    // Esto evita que otra interrupción interrumpa a esta misma mientras se procesa
    *IMEInter = false;

    // CORRECCIÓN CRÍTICA:
    // Debemos LIMPIAR (Apagar) el bit de la interrupción en el registro IF (0xFF0F).
    // Antes estabas usando '|=' que lo dejaba encendido para siempre.
    // Usamos '&=' con el complemento (~) para apagar solo ese bit.
    uint8_t requestInterrupt = mmu->read8(0xFF0F);
    requestInterrupt &= ~(1 << bitToSearch);
    mmu->write8(0xFF0F, requestInterrupt);

    // Guardamos el PC actual en la pila (Stack)
    // Asumimos que tu clase MMU maneja el puntero de pila (SP) internamente en push()
    mmu->push(*pcInterr);

    // Saltamos al vector de interrupción correspondiente
    switch (bitToSearch)
    {
    case 0: // V-Blank
        *pcInterr = 0x40;
        //std::cout << "INT: V-Blank" << std::endl;
        break;
    case 1: // LCD
        *pcInterr = 0x48;
        // std::cout << "INT: LCD STAT" << std::endl;
        break;
    case 2: // Timer
        *pcInterr = 0x50;
        std::cout << "INT: Timer" << std::endl;
        break;
    case 3: // Serial
        *pcInterr = 0x58;
        break;
    case 4: // Joypad
        *pcInterr = 0x60;
        std::cout << "INT: Joypad" << std::endl;
        break;
    default:
        break;
    }

    // NOTA: Una interrupción real consume 20 ciclos de reloj (5 M-Cycles).
    // Deberías sumar estos ciclos en tu CPU::step si quieres precisión perfecta.
}