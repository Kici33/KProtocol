#include <kprotocol/kprotocol.hpp>
#include <iostream>

int main() {
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;

    // Register baseline packets
    kprotocol::register_baseline_packets(registry, translator);

    // If generated packets exist, generated.hpp will declare register_generated_packets
#ifdef KPROTOCOL_GENERATED
    kprotocol::register_generated_packets(registry);
#endif

    std::cout << "kprotocol consumer example: registry has " << registry.size() << " packet definitions\n";
    return 0;
}
