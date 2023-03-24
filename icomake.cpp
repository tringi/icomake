#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <map>

void report (DWORD code = GetLastError ()) {
    std::printf ("error ");
    std::printf ((code > 0x4000u) ? "0x%08X: " : "%u: ", code);

    LPTSTR text;
    FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                   NULL, code, 0, (LPTSTR) &text, 0, NULL);
    std::wprintf (L"%s\n", text);
}

struct IcoHeader {
    std::uint16_t magic0 = 0;
    std::uint16_t type1 = 1;
    std::uint16_t n = 0;
};
struct IcoEntry {
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t colors;
    std::uint8_t reserved0 = 0;
    std::uint16_t planes1 = 0; // 0 or 1
    std::uint16_t bpp;
    std::uint32_t size;
    std::uint32_t offset = 0;
};

struct key {
    unsigned int width; // 1 - 256
    unsigned int height; // 1 - 256
    unsigned int bpp; // 1,2,4,8,24,32

    unsigned long weight () const {
        
        // optimized order for modern systems
        //  - mostly everything now uses 32bpp so put those first, then 24, 8, etc.
        //  - Windows iterates over all icons, searching for best match of them all, stopping only on exact match
        //  - BUT keep 16,32,48 sizes at 4,8,32 bpp among first 9 for XP and earlier (these should NOT be PNGs)

        switch (this->bpp) {
            case 32:
                if (this->width == 48 && this->height == 48) return 11;
                if (this->width == 32 && this->height == 32) return 12;
                if (this->width == 16 && this->height == 16) return 13;
                if (this->width == 24 && this->height == 24) return 91;
                break;
            case 8:
                if (this->width == 48 && this->height == 48) return 21;
                if (this->width == 32 && this->height == 32) return 22;
                if (this->width == 16 && this->height == 16) return 23;
                if (this->width == 24 && this->height == 24) return 92;
                break;
            case 4:
                if (this->width == 48 && this->height == 48) return 31;
                if (this->width == 32 && this->height == 32) return 32;
                if (this->width == 16 && this->height == 16) return 33;
                if (this->width == 24 && this->height == 24) return 93;
                break;
            case 24:
                if (this->width == 48 && this->height == 48) return 41;
                if (this->width == 32 && this->height == 32) return 42;
                if (this->width == 16 && this->height == 16) return 43;
                if (this->width == 24 && this->height == 24) return 94;
                break;
        }
        return ((this->width + this->height) << 8)
            | (this->height << 0)
            | ((32 - this->bpp) << 24);
    }
    bool operator < (const key & other) const {
        return this->weight () < other.weight ();
    }
    void apply (IcoEntry * entry) const {
        entry->width = (std::uint8_t) ((this->width == 256) ? 0 : this->width);
        entry->height = (std::uint8_t) ((this->height == 256) ? 0 : this->height);
        entry->bpp = (std::uint16_t) this->bpp;
    }
};
struct value {
    std::FILE * file = nullptr;
    const char * source = nullptr;
    std::uint32_t size;
    std::uint32_t offset;
    std::uint32_t target;
    std::uint16_t planes = 0; // 0 or 1
    std::uint8_t colors = 0;
    std::uint8_t reserved = 0;

    void apply (IcoEntry * entry) const {
        entry->colors = this->colors;
        entry->reserved0 = this->reserved;
        entry->planes1 = this->planes;
        entry->size = this->size;
        entry->offset = this->target;
    }
};

std::map <key, value> data;

static const std::uint8_t IcoHeaderBytes [] = { 0x00, 0x00, 0x01, 0x00 };
static const std::uint8_t PngHeaderBytes [] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

int main (int argc, char ** argv) {
    if (argc < 3) {
        std::printf ("Combines multiple ICO/PNGs into single ICO file, retaining format of each sub-image and optimizing order.\n");
        std::printf ("If multiple images for a given format are provided, the last one is used.\n");
        std::printf ("\n");
        std::printf ("Usage: %s output.ico input1.png [input2.ico] [...]\n", argv [0]);
    } else {
        std::printf ("Gathering:\n");
        for (auto source = &argv [2]; source != &argv [argc]; ++source) {
            std::printf (" [%s]:", *source);

            bool ok = false;
            bool msg = true;

            if (auto f = std::fopen (*source, "rb")) {

                unsigned char header [32];
                if (auto n = std::fread (header, 1, sizeof header, f)) {

                    // PNG?
                    if ((n >= 29)
                        && (std::memcmp (header, PngHeaderBytes, sizeof PngHeaderBytes) == 0)
                        && (std::strncmp ((const char *) &header [12], "IHDR", 4) == 0)) {

                        unsigned int width = (header [16] << 24) | (header [17] << 16) | (header [18] << 8) | (header [19] << 0);
                        unsigned int height = (header [20] << 24) | (header [21] << 16) | (header [22] << 8) | (header [23] << 0);

                        std::printf (" %ux%ux%u", width, height, header [24] * 4);

                        if (header [25] == 6) {
                            if ((width <= 256) && (height <= 256)) {

                                std::fseek (f, 0, SEEK_END);

                                data [{width, height, 32}] = { f, *source, (std::uint32_t) std::ftell (f), 0 };
                                ok = true;
                            } else {
                                std::printf (": image too big, max 256x256");
                                msg = false;
                            }
                        } else {
                            std::printf (": unsupported, use only true color PNGs");
                            msg = false;
                        }
                    }

                    // ICO
                    if ((n >= 22)
                        && (std::memcmp (header, IcoHeaderBytes, sizeof IcoHeaderBytes) == 0)) {

                        static IcoEntry entries [65535];
                        auto number = header [4] + (header [5] << 8);
                        
                        std::printf (" %u icons:", number);
                        std::fseek (f, sizeof (IcoHeader), SEEK_SET);

                        if ((number != 0) && (std::fread (entries, sizeof (IcoEntry), number, f) == number)) {

                            for (auto entry = &entries [0]; entry != &entries [number]; ++entry) {

                                unsigned int width = entry->width ? entry->width : 256;
                                unsigned int height = entry->height ? entry->height : 256;

                                if (entry != &entries [0]) {
                                    std::printf (",");
                                }
                                std::printf (" %ux%ux%u", width, height, entry->bpp);

                                data [{width, height, entry->bpp}] = { f, *source,
                                                                       entry->size, entry->offset,
                                                                       entry->planes1, entry->colors, entry->reserved0
                                                                     };
                            }
                            ok = true;

                        } else {
                            std::printf (" truncated!");
                            msg = false;
                        }
                    }

                    if (!ok && msg) {
                        std::printf (" unsupported type");
                        msg = false;
                    }
                    std::printf ("\n");
                }

                if (!ok) {
                    std::fclose (f);
                }
            }
            if (!ok && msg) {
                report ();
            }
        }

        if (!data.empty ()) {
            std::printf ("Compiling: ");

            { // compute offsets
                auto offset = (std::uint32_t) (sizeof (IcoHeader) + data.size () * sizeof (IcoEntry));
                for (auto & icon : data) {
                    icon.second.target = offset;
                    offset += icon.second.size;
                }
            }

            if (auto f = std::fopen (argv[1], "wb")) {
                {
                    IcoHeader header;
                    header.n = (std::uint16_t) data.size ();
                    std::fwrite (&header, sizeof header, 1, f);
                }

                for (const auto & [k, v] : data) {
                    IcoEntry entry;
                    k.apply (&entry);
                    v.apply (&entry);
                    std::fwrite (&entry, sizeof entry, 1, f);
                }

                std::printf ("%u icons...\n", (unsigned int) data.size ());

                for (const auto & [ico, src] : data) {
                    char page [65536];

                    std::fseek (src.file, src.offset, SEEK_SET);

                    const char * type = "UNK";
                    if (std::fread (page, sizeof PngHeaderBytes, 1, src.file)) {
                        if (std::memcmp (page, PngHeaderBytes, sizeof PngHeaderBytes) == 0) {
                            type = "PNG";
                        } else {
                            type = "ICO";
                        }
                    }

                    std::printf (" [%ux%ux%u] %s (%s) from %08x:%u to %08x\n",
                                 ico.width, ico.height, ico.bpp, src.source, type, src.offset, src.size, src.target);

                    auto offset = sizeof PngHeaderBytes;
                    auto amount = src.size;
                    while (amount >= sizeof page) {
                        if (std::fread (page + offset, sizeof page - offset, 1, src.file) && std::fwrite (page, sizeof page, 1, f)) {
                            amount -= sizeof page;
                            offset = 0;
                        } else
                            goto failed;
                    }
                    if (amount != 0) {
                        if (!std::fread (page + offset, amount - offset, 1, src.file) || !std::fwrite (page, amount, 1, f))
                            goto failed;
                    }
                }
           
                std::printf ("Done.\n");
            } else {
failed:
                report ();
            }
        } else {
            std::printf ("no valid inputs\n");
        }
    }
    return 0;
}
