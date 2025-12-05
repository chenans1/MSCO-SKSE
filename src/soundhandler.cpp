#include "PCH.h"
#include "soundhandler.h"

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
}