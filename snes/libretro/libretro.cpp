#include "libretro.h"
#include <snes.hpp>

#include <nall/snes/info.hpp>
using namespace nall;

// Special memory types.
#define RETRO_MEMORY_SNES_BSX_RAM             ((1 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_BSX_PRAM            ((2 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM  ((3 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM  ((4 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RAM        ((5 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RTC        ((6 << 8) | RETRO_MEMORY_RTC)

// Special game types passed into retro_load_game_special().
// Only used when multiple ROMs are required.
#define RETRO_GAME_TYPE_BSX             0x101
#define RETRO_GAME_TYPE_BSX_SLOTTED     0x102
#define RETRO_GAME_TYPE_SUFAMI_TURBO    0x103
#define RETRO_GAME_TYPE_SUPER_GAME_BOY  0x104

#define RETRO_DEVICE_JOYPAD_MULTITAP       RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 0)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIER    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 1)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIERS   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 2)


struct Interface : public SNES::Interface {
	
	
  retro_video_refresh_t pvideo_refresh;
  retro_audio_sample_batch_t paudio_sample;
  retro_input_poll_t pinput_poll;
  retro_input_state_t pinput_state;
  retro_environment_t penviron;	
  bool overscan;
  uint32_t palette[32768];
  uint32_t *buffer;

  void video_refresh(const uint16_t *data, unsigned width, unsigned height) { 
	  
	bool interlace = (height >= 240);
    unsigned pitch = 1024 >> interlace;
    if(interlace) height <<= 1;
   // data += 9 * 1024;  //skip front porch
	 unsigned outpitch =  1024 * 4;
	for(unsigned y = 0; y < height; y++) {
      uint32_t *output = buffer + y * (outpitch >> 2);
      const uint16_t *input = data + y * pitch;
      for(unsigned x = 0; x < width; x++) *output++ = palette[*input++];
    }
      
    pvideo_refresh(buffer, width, height, outpitch);
  }

  void audio_sample(uint16_t left, uint16_t right) {
	const int16_t samples[2] = { left, right };
    paudio_sample(samples, 1);
  }

  void input_poll() {
    if(pinput_poll) return pinput_poll();
  }

  int16_t input_poll(bool port, SNES::Input::Device device, unsigned index, unsigned id) {
    if(pinput_state) return pinput_state(port, (unsigned)device, index, id);
    return 0;
  }
  
  void palette_gen() {
  for(unsigned i = 0; i < 32768; i++) {
    unsigned r = (i >> 10) & 31;
    unsigned g = (i >>  5) & 31;
    unsigned b = (i >>  0) & 31;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
   palette[i] = (r << 16) | (g << 8) | (b << 0);
  }
}

  Interface() : pvideo_refresh(0), paudio_sample(0), pinput_poll(0), pinput_state(0) {
    buffer = new uint32_t[512 * 480];
	palette_gen();
  }
  
  ~Interface() {
    delete[] buffer;
  }
};

static Interface interface;

unsigned retro_api_version(void) {
  return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t environ_cb)
{
   interface.penviron       = environ_cb;
}

void retro_set_video_refresh(retro_video_refresh_t video_refresh) {  interface.pvideo_refresh = video_refresh; }
void retro_set_audio_sample(retro_audio_sample_t)    { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t audio_sample) { interface.paudio_sample = audio_sample; }
void retro_set_input_poll(retro_input_poll_t input_poll)          {  interface.pinput_poll = input_poll; }
void retro_set_input_state(retro_input_state_t input_state)       {  interface.pinput_state = input_state; }

void retro_set_controller_port_device(unsigned port, unsigned device) {
  if (port < 2)
     SNES::input.port_set_device(port, (SNES::Input::Device)device);
}



void retro_deinit(void) {
  SNES::system.term();
}

void retro_init(void) {
  SNES::system.init(&interface);
  SNES::input.port_set_device(0, SNES::Input::Device::Joypad);
  SNES::input.port_set_device(1, SNES::Input::Device::Joypad);
}

void retro_reset(void) {
  SNES::system.reset();
}

void retro_run(void) {
  SNES::system.run();
}

size_t retro_serialize_size(void) {
  return SNES::system.serialize_size();
}

bool retro_serialize(void *data, size_t size) {
  SNES::system.runtosave();
  serializer s = SNES::system.serialize();
  if(s.size() > size) return false;
  memcpy((void*)data, s.data(), s.size());
  return true;
}

bool retro_unserialize(const void *data, size_t size) {
  serializer s((uint8_t*)data, size);
  return SNES::system.unserialize(s);
}

void retro_cheat_reset(void) {
  SNES::cheat.reset();
  SNES::cheat.synchronize();
}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
  SNES::cheat[index] = code;
  SNES::cheat[index].enabled = enabled;
  SNES::cheat.synchronize();
}


void retro_get_system_info(struct retro_system_info *info) {
  static string version("v1.51");
  info->library_name     = "bzsnes";
  info->library_version  = version;
  info->valid_extensions = "sfc|smc";
  info->need_fullpath    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
 struct retro_system_timing timing = { 0.0, 32040.5 };
  timing.fps = retro_get_region() == RETRO_REGION_NTSC ? 21477272.0 / 357366.0 : 21281370.0 / 425568.0;

  if (!interface.penviron(RETRO_ENVIRONMENT_GET_OVERSCAN, &interface.overscan))
     interface.overscan = false;

  struct retro_game_geometry geom = { 256, interface.overscan ? 239 : 224, 512, interface.overscan ? 478 : 448 };

  enum retro_pixel_format fmt;
  fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  interface.penviron(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
  
  info->timing   = timing;
  info->geometry = geom;
  
 }

bool snes_load_cartridge_normal(
  const char *rom_xml, const uint8_t *rom_data, unsigned rom_size
) {
  retro_cheat_reset();
  if(rom_data) SNES::memory::cartrom.copy(rom_data, rom_size);
  string xmlrom = (rom_xml && *rom_xml) ? string(rom_xml) : snes_information(rom_data, rom_size).xml_memory_map;
  SNES::cartridge.load(SNES::Cartridge::Mode::Normal, { xmlrom });
  SNES::system.power();
  return true;
}

bool snes_load_cartridge_bsx_slotted(
  const char *rom_xml, const uint8_t *rom_data, unsigned rom_size,
  const char *bsx_xml, const uint8_t *bsx_data, unsigned bsx_size
) {
  retro_cheat_reset();
  if(rom_data) SNES::memory::cartrom.copy(rom_data, rom_size);
  string xmlrom = (rom_xml && *rom_xml) ? string(rom_xml) : snes_information(rom_data, rom_size).xml_memory_map;
  if(bsx_data) SNES::memory::bsxflash.copy(bsx_data, bsx_size);
  string xmlbsx = (bsx_xml && *bsx_xml) ? string(bsx_xml) : snes_information(bsx_data, bsx_size).xml_memory_map;
  SNES::cartridge.load(SNES::Cartridge::Mode::BsxSlotted, { xmlrom, xmlbsx });
  SNES::system.power();
  return true;
}

bool snes_load_cartridge_bsx(
  const char *rom_xml, const uint8_t *rom_data, unsigned rom_size,
  const char *bsx_xml, const uint8_t *bsx_data, unsigned bsx_size
) {
  retro_cheat_reset();
  if(rom_data) SNES::memory::cartrom.copy(rom_data, rom_size);
  string xmlrom = (rom_xml && *rom_xml) ? string(rom_xml) : snes_information(rom_data, rom_size).xml_memory_map;
  if(bsx_data) SNES::memory::bsxflash.copy(bsx_data, bsx_size);
  string xmlbsx = (bsx_xml && *bsx_xml) ? string(bsx_xml) : snes_information(bsx_data, bsx_size).xml_memory_map;
  SNES::cartridge.load(SNES::Cartridge::Mode::Bsx, { xmlrom, xmlbsx });
  SNES::system.power();
  return true;
}

bool snes_load_cartridge_sufami_turbo(
  const char *rom_xml, const uint8_t *rom_data, unsigned rom_size,
  const char *sta_xml, const uint8_t *sta_data, unsigned sta_size,
  const char *stb_xml, const uint8_t *stb_data, unsigned stb_size
) {
  retro_cheat_reset();
  if(rom_data) SNES::memory::cartrom.copy(rom_data, rom_size);
  string xmlrom = (rom_xml && *rom_xml) ? string(rom_xml) : snes_information(rom_data, rom_size).xml_memory_map;
  if(sta_data) SNES::memory::stArom.copy(sta_data, sta_size);
  string xmlsta = (sta_xml && *sta_xml) ? string(sta_xml) : snes_information(sta_data, sta_size).xml_memory_map;
  if(stb_data) SNES::memory::stBrom.copy(stb_data, stb_size);
  string xmlstb = (stb_xml && *stb_xml) ? string(stb_xml) : snes_information(stb_data, stb_size).xml_memory_map;
  SNES::cartridge.load(SNES::Cartridge::Mode::SufamiTurbo, { xmlrom, xmlsta, xmlstb });
  SNES::system.power();
  return true;
}

bool snes_load_cartridge_super_game_boy(
  const char *rom_xml, const uint8_t *rom_data, unsigned rom_size,
  const char *dmg_xml, const uint8_t *dmg_data, unsigned dmg_size
) {
  retro_cheat_reset();
  if(rom_data) SNES::memory::cartrom.copy(rom_data, rom_size);
  string xmlrom = (rom_xml && *rom_xml) ? string(rom_xml) : snes_information(rom_data, rom_size).xml_memory_map;
  if(dmg_data) SNES::memory::gbrom.copy(dmg_data, dmg_size);
  string xmldmg = (dmg_xml && *dmg_xml) ? string(dmg_xml) : snes_information(dmg_data, dmg_size).xml_memory_map;
  SNES::cartridge.load(SNES::Cartridge::Mode::SuperGameBoy, { xmlrom, xmldmg });
  SNES::system.power();
  return true;
}

static void init_descriptors(void)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,   "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,   "Select" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },

      { 0 },
   };

   interface.penviron(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

bool retro_load_game(const struct retro_game_info *info) {
  retro_cheat_reset();
  init_descriptors();

  return snes_load_cartridge_normal(NULL, (const uint8_t*)info->data, info->size);
}

bool retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info) {
  retro_cheat_reset();
  init_descriptors();

  switch (game_type) {
      case RETRO_GAME_TYPE_BSX:
       return num_info == 2 && snes_load_cartridge_bsx(info[0].meta, (const uint8_t*)info[0].data, info[0].size,
             info[1].meta, (const uint8_t*)info[1].data, info[1].size);
       
     case RETRO_GAME_TYPE_BSX_SLOTTED:
       return num_info == 2 && snes_load_cartridge_bsx_slotted(info[0].meta, (const uint8_t*)info[0].data, info[0].size,
             info[1].meta, (const uint8_t*)info[1].data, info[1].size);

     case RETRO_GAME_TYPE_SUPER_GAME_BOY:
       return num_info == 2 && snes_load_cartridge_super_game_boy(info[0].meta, (const uint8_t*)info[0].data, info[0].size,
             info[1].meta, (const uint8_t*)info[1].data, info[1].size);

     case RETRO_GAME_TYPE_SUFAMI_TURBO:
       return num_info == 3 && snes_load_cartridge_sufami_turbo(info[0].meta, (const uint8_t*)info[0].data, info[0].size,
             info[1].meta, (const uint8_t*)info[1].data, info[1].size,
             info[2].meta, (const uint8_t*)info[2].data, info[2].size);

     default:
        return false;
  }
}


void retro_unload_game(void) {
 SNES::cartridge.unload();
}

unsigned retro_get_region(void) {
  return SNES::system.region() == SNES::System::Region::NTSC ? 0 : 1;
}

void* retro_get_memory_data(unsigned id) {
  if(SNES::cartridge.loaded() == false) return NULL;

  switch(id) {
    case RETRO_MEMORY_SAVE_RAM:
      return SNES::memory::cartram.data();
    case RETRO_MEMORY_RTC:
      return SNES::memory::cartrtc.data();
    case RETRO_MEMORY_SNES_BSX_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::Bsx) break;
      return SNES::memory::bsxram.data();
    case RETRO_MEMORY_SNES_BSX_PRAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::Bsx) break;
      return SNES::memory::bsxpram.data();
    case RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SufamiTurbo) break;
      return SNES::memory::stAram.data();
    case RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SufamiTurbo) break;
      return SNES::memory::stBram.data();
    case RETRO_MEMORY_SNES_GAME_BOY_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SuperGameBoy) break;
      SNES::supergameboy.save();
      return SNES::memory::gbram.data();
  }

  return NULL;
}

size_t retro_get_memory_size(unsigned id) {
  if(SNES::cartridge.loaded() == false) return 0;
  unsigned size = 0;

  switch(id) {
    case RETRO_MEMORY_SAVE_RAM:
      size = SNES::memory::cartram.size();
      break;
    case RETRO_MEMORY_RTC:
      size = SNES::memory::cartrtc.size();
      break;
    case RETRO_MEMORY_SNES_BSX_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::Bsx) break;
      size = SNES::memory::bsxram.size();
      break;
    case RETRO_MEMORY_SNES_BSX_PRAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::Bsx) break;
      size = SNES::memory::bsxpram.size();
      break;
    case RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SufamiTurbo) break;
      size = SNES::memory::stAram.size();
      break;
    case RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SufamiTurbo) break;
      size = SNES::memory::stBram.size();
      break;
    case RETRO_MEMORY_SNES_GAME_BOY_RAM:
      if(SNES::cartridge.mode() != SNES::Cartridge::Mode::SuperGameBoy) break;
      size = SNES::memory::gbram.size();
      break;
  }

  if(size == -1U) size = 0;
  return size;
}
