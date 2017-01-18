#include <iostream>
#include <fstream>
#include <SDL.h>
#include <unistd.h>

#define WIN_W 64
#define WIN_H 32
#define WIN_SCALE 16

#define OPX(op) ((op & 0x0F00) >> 8)
#define OPY(op) ((op & 0x00F0) >> 4)

unsigned char chip8_fontset[80] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

class Chip8 {
private:
  unsigned short opcode;

  unsigned char  memory[4096];
  unsigned char  gfx_memory[32][64];

  unsigned char  reg[16];
  unsigned short I;
  unsigned short pc;

  unsigned char delay_timer;
  unsigned char sound_timer;

  unsigned short stack[16];
  unsigned short sp;

  unsigned char keys[16];

  void cpu_zero_opcode(unsigned short opcode) {
    switch(opcode) {
    case 0x00E0:
      for (int i = 0; i < 32; i++)
        for (int n = 0; n < 64; n++)
          gfx_memory[i][n] = 0;

      pc += 2;
      break;
    case 0x00EE:
      sp--;
      pc = stack[sp];
      pc += 2;
      break;
    default:
      // Ignore Call RCA 1802
      break;
    }
  }

  void cpu_arithmetic_opcode(unsigned short opcode) {
    switch(opcode & 0x000F) {
    case 0x0000:
      reg[OPX(opcode)] = reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0001:
      reg[OPX(opcode)] = reg[OPX(opcode)] | reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0002:
      reg[OPX(opcode)] = reg[OPX(opcode)] & reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0003:
      reg[OPX(opcode)] = reg[OPX(opcode)] ^ reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0004:
      reg[OPX(opcode)] += reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0005:
      reg[OPX(opcode)] -= reg[OPY(opcode)];
      pc += 2;
      break;
    case 0x0006:
      reg[OPX(opcode)] = reg[OPX(opcode)] >> 1;
      pc += 2;
      break;
    case 0x0007:
      reg[OPX(opcode)] = reg[OPY(opcode)] - reg[OPX(opcode)];
      pc += 2;
      break;
    case 0x000E:
      reg[OPX(opcode)] = reg[OPX(opcode)] << 1;
      pc += 2;
      break;
    }
  }

  void draw_sprite(unsigned short x, unsigned short y, unsigned short n) {
    unsigned short pixel;
    for (int yline = 0; yline < n; yline++) {
      pixel = memory[I+yline];
      for (int xline = 0; xline < 8; xline++)
        if ((pixel & (0x80 >> xline)) != 0) {
          if (gfx_memory[y+yline][x+xline] == 1)
            reg[15] = 1;
          gfx_memory[y+yline][x+xline] ^= 1;
	}
    }
  }

  void cpu_key_opcode(unsigned short opcode) {
    switch (opcode & 0x00FF) {
    case 0x009E:
      if (keys[reg[OPX(opcode)]] == 1) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;
    case 0x00A1:
      if (keys[reg[OPX(opcode)]] != 1) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;
    }
  }

  unsigned short sprite_addr(unsigned short opcode) {
    // store sprite addr of char in Vx to I
    switch (reg[OPX(opcode)]) {
    case 0x0:
      return 0;
    case 0x1:
      return 5;
    case 0x2:
      return 10;
    case 0x3:
      return 15;
    case 0x4:
      return 20;
    case 0x5:
      return 25;
    case 0x6:
      return 30;
    case 0x7:
      return 35;
    case 0x8:
      return 40;
    case 0x9:
      return 45;
    case 0xA:
      return 50;
    case 0xB:
      return 55;
    case 0xC:
      return 60;
    case 0xD:
      return 65;
    case 0xE:
      return 70;
    case 0xF:
      return 75;
    default:
      return 0;
    }
  }

  void cpu_f_opcode(unsigned short opcode) {
    switch(opcode & 0x00FF) {
    case 0x0007:
      reg[OPX(opcode)] = delay_timer;
      pc += 2;
      break;
    case 0x000A:
      // wait for keypress
      pc += 2;
      break;
    case 0x0015:
      delay_timer = reg[OPX(opcode)];
      pc += 2;
      break;
    case 0x0018:
      sound_timer = reg[OPX(opcode)];
      pc += 2;
      break;
    case 0x001E:
      I += reg[OPX(opcode)];
      pc += 2;
      break;
    case 0x0029:
      I = sprite_addr(OPX(opcode));
      pc += 2;
      break;
    case 0x0033:
      memory[I]     = reg[(opcode & 0x0F00) >> 8] / 100;
      memory[I + 1] = (reg[(opcode & 0x0F00) >> 8] / 10) % 10;
      memory[I + 2] = (reg[(opcode & 0x0F00) >> 8] % 100) % 10;
      pc += 2;
      break;
    case 0x0055:
      for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
        memory[I+i] = reg[i];
      pc += 2;
      break;
    case 0x0065:
      for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
        reg[i] = memory[I+i];
      pc += 2;
      break;
    }
  }

  void cpu_opcode(unsigned short opcode) {
    std::cout << "0x" << std::hex  << std::uppercase << pc << ": ";
    std::cout << "0x" << std::hex << std::uppercase << opcode << std::endl;
    switch (opcode & 0xF000) {
    case 0x0000:
      cpu_zero_opcode(opcode);
      break;
    case 0x1000:
      pc = opcode & 0x0FFF;
      break;
    case 0x2000:
      stack[sp] = pc;
      sp++;
      pc = opcode & 0x0FFF;
      break;
    case 0x3000:
      if (reg[OPX(opcode)] == (opcode & 0x00FF))
        pc += 4;
      else
        pc += 2;
      break;
    case 0x4000:
      if (reg[OPX(opcode)] != (opcode & 0x00FF))
        pc += 4;
      else
        pc += 2;
      break;
    case 0x5000:
      if (reg[OPX(opcode)] == reg[OPY(opcode)])
        pc += 4;
      else
        pc += 2;
      break;
    case 0x6000:
      reg[OPX(opcode)] = (opcode & 0x00FF);
      pc += 2;
      break;
    case 0x7000:
      reg[OPX(opcode)] += (opcode & 0x00FF);
      pc += 2;
      break;
    case 0x8000:
      cpu_arithmetic_opcode(opcode);
      break;
    case 0x9000:
      if (reg[OPX(opcode)] != reg[OPY(opcode)])
        pc += 4;
      else
        pc += 2;
      break;
    case 0xA000:
      I = opcode & 0x0FFF;
      pc += 2;
      break;
    case 0xB000:
      pc = reg[0] + (opcode & 0x0FFF);
      break;
    case 0xC000:
      reg[OPX(opcode)] = 4 & (opcode & 0x00FF);
      pc += 2;
      break;
    case 0xD000:
      draw_sprite(reg[OPX(opcode)], reg[OPY(opcode)], opcode & 0x000F);
      pc += 2;
      break;
    case 0xE000:
      cpu_key_opcode(opcode);
      break;
    case 0xF000:
      cpu_f_opcode(opcode);
      break;
    }
  }
public:
  Chip8() {
    pc = 0x200;
    opcode = 0;
    I = 0;
    sp = 0;

    clear_display();
    for (int i = 0; i < 80; ++i)
      memory[i] = chip8_fontset[i];

    for (int i = 0; i < 16; i++) {
      stack[i] = 0;
      reg[i] = 0;
    }
  }

  void clear_display() {
    for (int i = 0; i < 32; i++)
      for (int n = 0; n < 64; n++)
        gfx_memory[i][n] = 0;
  }

  void step(SDL_Renderer *renderer) {
    // Fetch Opcode
    opcode = memory[pc] << 8 | memory[pc+1];
    // Decode/Execute Opcode
    cpu_opcode(opcode);
    // Update system timers
    sound_timer--;
    delay_timer--;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < 32; i++)
      for (int n = 0; n < 64; n++)
        if (gfx_memory[i][n] == 1)
          SDL_RenderDrawPoint(renderer, n, i);
    SDL_RenderPresent(renderer);
  }

  void load_rom(const char* filename) {
    auto file = std::ifstream(filename, std::ios::binary | std::ios::ate);
    auto size = file.tellg();
    file.seekg(0);
    file.read(reinterpret_cast<char*>(memory + 0x200), size);
  }

  void key_down(int key) {
    keys[key] = 1;
  }

  void key_up(int key) {
    keys[key] = 0;
  }
};


int main(int argc, char** argv) {
  (void) argc;
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_CreateWindowAndRenderer(WIN_W*WIN_SCALE, WIN_H*WIN_SCALE, 0, &window, &renderer);
  // set up input
  auto chip8 = Chip8();
  chip8.load_rom(argv[1]);

  SDL_RenderSetScale(renderer, WIN_SCALE, WIN_SCALE);
  SDL_Event evt;
  bool quit = 0;
  while (!quit) {
    SDL_PollEvent(&evt);
    switch(evt.type) {
    case SDL_QUIT:
      quit = true;
      break;
    case SDL_KEYDOWN:
      switch (evt.key.keysym.sym) {
      case SDLK_1:
        chip8.key_down(0x1);
        break;
      case SDLK_2:
        chip8.key_down(0x2);
        break;
      case SDLK_3:
        chip8.key_down(0x3);
        break;
      case SDLK_4:
        chip8.key_down(0xC);
        break;
      case SDLK_q:
        chip8.key_down(0x4);
        break;
      case SDLK_w:
        chip8.key_down(0x5);
        break;
      case SDLK_e:
        chip8.key_down(0x6);
        break;
      case SDLK_r:
        chip8.key_down(0xD);
        break;
      case SDLK_a:
        chip8.key_down(0x7);
        break;
      case SDLK_s:
        chip8.key_down(0x8);
        break;
      case SDLK_d:
        chip8.key_down(0x9);
        break;
      case SDLK_f:
        chip8.key_down(0xE);
        break;
      case SDLK_z:
        chip8.key_down(0xA);
        break;
      case SDLK_x:
        chip8.key_down(0x0);
        break;
      case SDLK_c:
        chip8.key_down(0xB);
        break;
      case SDLK_v:
        chip8.key_down(0xF);
        break;
      }
      chip8.step(renderer);
      break;
    case SDL_KEYUP:
      switch (evt.key.keysym.sym) {
      case SDLK_1:
        chip8.key_up(0x1);
        break;
      case SDLK_2:
        chip8.key_up(0x2);
        break;
      case SDLK_3:
        chip8.key_up(0x3);
        break;
      case SDLK_4:
        chip8.key_up(0xC);
        break;
      case SDLK_q:
        chip8.key_up(0x4);
        break;
      case SDLK_w:
        chip8.key_up(0x5);
        break;
      case SDLK_e:
        chip8.key_up(0x6);
        break;
      case SDLK_r:
        chip8.key_up(0xD);
        break;
      case SDLK_a:
        chip8.key_up(0x7);
        break;
      case SDLK_s:
        chip8.key_up(0x8);
        break;
      case SDLK_d:
        chip8.key_up(0x9);
        break;
      case SDLK_f:
        chip8.key_up(0xE);
        break;
      case SDLK_z:
        chip8.key_up(0xA);
        break;
      case SDLK_x:
        chip8.key_up(0x0);
        break;
      case SDLK_c:
        chip8.key_up(0xB);
        break;
      case SDLK_v:
        chip8.key_up(0xF);
        break;
      }
      chip8.step(renderer);
      break;
    default:
      chip8.step(renderer);
    }
    usleep(500);
  }

  // Cleanup
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
