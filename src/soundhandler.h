#pragma once

#include "PCH.h"
#include "soundhandler.h"

namespace MSCO::Sound {
    RE::BSSoundHandle play_sound(RE::Actor* actor, RE::BGSSoundDescriptorForm* sound_form);
}