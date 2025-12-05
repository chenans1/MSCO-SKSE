#include "PCH.h"
#include "soundhandler.h"
using namespace SKSE;
using namespace SKSE::log;

namespace MSCO::Sound {
	//plays sound if it exists on the actor. adapted from DTRY's payload and spell hotbar2
    RE::BSSoundHandle play_sound(RE::Actor* actor, RE::BGSSoundDescriptorForm* sound_form) {
		RE::BSSoundHandle handle;
        handle.soundID = static_cast<uint32_t>(-1);
        handle.assumeSuccess = false;
        handle.state = RE::BSSoundHandle::AssumedState::kInitialized;
        /*assumption: not doing this causes the game to crash if the audio engine is paused,
        eg: always active, mute on focus loss mod installed, tab out during casting*/
        auto audio_manager = RE::BSAudioManager::GetSingleton();
        if (audio_manager) {
            //16 is used by payload & spellhotbar 2
            audio_manager->BuildSoundDataFromDescriptor(handle, sound_form, 16);
            if (handle.SetPosition(actor->data.location)) {
                handle.SetObjectToFollow(actor->Get3D());
                handle.Play();
            }
        }
        return handle;
	}

    //get the release sound of the mgef
    RE::BGSSoundDescriptorForm* GetMGEFSound(
        RE::MagicItem* magic,
        RE::MagicSystem::SoundID soundID = RE::MagicSystem::SoundID::kRelease) 
    {
        if (!magic) {
            log::warn("GetReleaseSound called on null magic item");
            return nullptr;
        }
        if (auto* spell = magic->As<RE::SpellItem>()) {
            auto& effects = spell->effects;
            for (auto& effect : effects) {
                if (!effect || !effect->baseEffect) {
                    continue; //only grab the sound from the baseffect
                }

                auto& effectSounds = effect->baseEffect->effectSounds;
                for (const auto& effectSound : effectSounds) {
                    if (effectSound.id == RE::MagicSystem::SoundID::kRelease && effectSound.sound) {
                        return effectSound.sound;
                    }
                }
            }
        }
    }

}