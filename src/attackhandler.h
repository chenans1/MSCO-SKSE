#include <cstring>
#include <RE/A/AttackBlockHandler.h>
#include <RE/B/ButtonEvent.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/PlayerControlsData.h>
#include <REL/Relocation.h>

namespace MSCO {

    class AttackBlockHook {
    public:
        static void Install();
    };
}