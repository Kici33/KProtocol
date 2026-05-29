#include "kprotocol/initialize.hpp"

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/generated.hpp"
#include "kprotocol/translation_registry.hpp"

namespace kprotocol {

void initialize(PacketRegistry& registry, PacketTranslator& translator) {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
    register_generated_packets(registry);
#endif
    register_baseline_packets(registry, translator);
    TranslationRegistry::initialize_all(translator);
}

} // namespace kprotocol
