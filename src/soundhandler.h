#pragma once

namespace MSCO::Sound {
    RE::BSSoundHandle play_sound(RE::Actor* actor, RE::BGSSoundDescriptorForm* sound_form);
    RE::BGSSoundDescriptorForm* GetMGEFSound(
        RE::MagicItem* magic, 
        RE::MagicSystem::SoundID soundID = RE::MagicSystem::SoundID::kRelease);
}