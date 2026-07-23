#include "keyboard.h"

#include "../driver/pic.h"
#include "linux/irq.h"
#include "string.h"

static keyboard_t KEYBOARD_INPUT;
extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void asm_irq_keyboard();
uint8_t scancode_to_ascii(uint8_t code, int shift);

void init_keyboard() {
  KEYBOARD_INPUT.head = 0;
  KEYBOARD_INPUT.tail = -1;
  memset(KEYBOARD_INPUT.input, '\0', KEYBOARD_INPUT_BUFFER_SIZE);

  outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_RESET);
  io_wait();
  if (inb(KEYBOARD_DATA_PORT) != KEYBOARD_RESPONSE_ACK) {
    outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_RESET);
    io_wait();
  }
  outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_ENABLE_SCANNING);
  io_wait();
  if (inb(KEYBOARD_DATA_PORT) != KEYBOARD_RESPONSE_ACK) {
    outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_ENABLE_SCANNING);
    io_wait();
  }

  __asm__ volatile("cli");
  set_idt_descriptor(KERNEL_IDT + IRQ_BASE + 1, (void*)asm_irq_keyboard, 0x08,
                     IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
  __asm__ volatile("sti");

  return;
}

uint8_t keyboard_get_key(){
  if(KEYBOARD_INPUT.tail >= KEYBOARD_INPUT.head){
    __asm__ volatile("cli");
    char current_key = KEYBOARD_INPUT.input[KEYBOARD_INPUT.head++];
    __asm__ volatile("sti");
    if(KEYBOARD_INPUT.head >= KEYBOARD_INPUT_BUFFER_SIZE){
      KEYBOARD_INPUT.head = 0;
    }
    return current_key;
  }
  return '\0';
}

void keyboard_clear(){
  __asm__ volatile("cli");
  KEYBOARD_INPUT.head = 0;
  KEYBOARD_INPUT.tail = -1;
  __asm__ volatile("sti");
}

void irq_keyboard(pt_regs* regs) {
  uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);

  uint8_t scan_code = inb(KEYBOARD_DATA_PORT);

  static int extended = 0;
  static int shift_pressed = 0;

  if (scan_code == 0xE0) {
    extended = 1;
  } else {
    int released = scan_code & 0x80;  // MSB=1 → 뗌(break)
    uint8_t code = scan_code & 0x7F;

    if(code == 0x2A || code == 0x36){
      shift_pressed = !released;
    }
    if (!released) {
      if (KEYBOARD_INPUT.tail >= KEYBOARD_INPUT_BUFFER_SIZE-1) {
        KEYBOARD_INPUT.tail = -1; // 초기화: 비어 있는 상태를 제외하고 항상 tail이 head 앞에 오도록
        KEYBOARD_INPUT.head = 0;
      }
      KEYBOARD_INPUT.input[++KEYBOARD_INPUT.tail] = scancode_to_ascii(code, shift_pressed);
    }

    extended = 0;
  }

  pic_send_eoi(irq);
}

uint8_t scancode_to_ascii(uint8_t code, int shift) {
  switch (code) {
    case 0x01:
      return 0x1B; /* Esc */
    case 0x02:
      return shift ? '!' : '1';
    case 0x03:
      return shift ? '@' : '2';
    case 0x04:
      return shift ? '#' : '3';
    case 0x05:
      return shift ? '$' : '4';
    case 0x06:
      return shift ? '%' : '5';
    case 0x07:
      return shift ? '^' : '6';
    case 0x08:
      return shift ? '&' : '7';
    case 0x09:
      return shift ? '*' : '8';
    case 0x0A:
      return shift ? '(' : '9';
    case 0x0B:
      return shift ? ')' : '0';
    case 0x0C:
      return shift ? '_' : '-';
    case 0x0D:
      return shift ? '+' : '=';
    case 0x0E:
      return '\b';
    case 0x0F:
      return '\t';
    case 0x10:
      return shift ? 'Q' : 'q';
    case 0x11:
      return shift ? 'W' : 'w';
    case 0x12:
      return shift ? 'E' : 'e';
    case 0x13:
      return shift ? 'R' : 'r';
    case 0x14:
      return shift ? 'T' : 't';
    case 0x15:
      return shift ? 'Y' : 'y';
    case 0x16:
      return shift ? 'U' : 'u';
    case 0x17:
      return shift ? 'I' : 'i';
    case 0x18:
      return shift ? 'O' : 'o';
    case 0x19:
      return shift ? 'P' : 'p';
    case 0x1A:
      return shift ? '{' : '[';
    case 0x1B:
      return shift ? '}' : ']';
    case 0x1C:
      return '\n';
    case 0x1E:
      return shift ? 'A' : 'a';
    case 0x1F:
      return shift ? 'S' : 's';
    case 0x20:
      return shift ? 'D' : 'd';
    case 0x21:
      return shift ? 'F' : 'f';
    case 0x22:
      return shift ? 'G' : 'g';
    case 0x23:
      return shift ? 'H' : 'h';
    case 0x24:
      return shift ? 'J' : 'j';
    case 0x25:
      return shift ? 'K' : 'k';
    case 0x26:
      return shift ? 'L' : 'l';
    case 0x27:
      return shift ? ':' : ';';
    case 0x28:
      return shift ? '"' : '\'';
    case 0x29:
      return shift ? '~' : '`';
    case 0x2B:
      return shift ? '|' : '\\';
    case 0x2C:
      return shift ? 'Z' : 'z';
    case 0x2D:
      return shift ? 'X' : 'x';
    case 0x2E:
      return shift ? 'C' : 'c';
    case 0x2F:
      return shift ? 'V' : 'v';
    case 0x30:
      return shift ? 'B' : 'b';
    case 0x31:
      return shift ? 'N' : 'n';
    case 0x32:
      return shift ? 'M' : 'm';
    case 0x33:
      return shift ? '<' : ',';
    case 0x34:
      return shift ? '>' : '.';
    case 0x35:
      return shift ? '?' : '/';
    case 0x37:
      return '*'; /* keypad * */
    case 0x39:
      return ' ';
    case 0x4A:
      return '-'; /* keypad - */
    case 0x4E:
      return '+'; /* keypad + */
    default:
      return '\0';
  }
}