#ifndef DWARF_H
#define DWARF_H

#include "elfinfo.h"
#include <sys/ucontext.h>
#include <map>
#include <list>
#include <vector>
#include <string>

#define DWARF_MAXREG 128

class Process;

enum DwarfHasChildren { DW_CHILDREN_yes = 1, DW_CHILDREN_no = 0 };
template <typename Elf> struct DwarfCIE;
template <typename Elf> struct DwarfInfo;
template <typename Elf> class DWARFReader;
template <typename Elf> class DwarfLineInfo;
template <typename Elf> struct DwarfUnit;
template <typename Elf> struct DwarfFrameInfo;
template <typename Elf> struct DwarfEntry;

typedef struct {
    uintmax_t reg[DWARF_MAXREG];
} DwarfRegisters;

#define DWARF_TAG(a,b) a = b,
enum DwarfTag {
#include "dwarf/tags.h"
    DW_TAG_none = 0x0
};
#undef DWARF_TAG

#define DWARF_FORM(a,b) a = b,
enum DwarfForm {
#include "dwarf/forms.h"
    DW_FORM_none = 0x0
};
#undef DWARF_FORM

#define DWARF_ATTR(a,b) a = b,
enum DwarfAttrName {
#include "dwarf/attr.h"
    DW_AT_none = 0x0
};
#undef DWARF_ATTR

#define DWARF_LINE_S(a,b) a = b,
enum DwarfLineSOpcode {
#include "dwarf/line_s.h"
    DW_LNS_none = -1
};
#undef DWARF_LINE_S

#define DWARF_LINE_E(a,b) a = b,
enum DwarfLineEOpcode {
#include "dwarf/line_e.h"
    DW_LNE_none = -1
};
#undef DWARF_LINE_E

struct DwarfAttributeSpec {
    enum DwarfAttrName name;
    enum DwarfForm form;
    DwarfAttributeSpec(DwarfAttrName name_, DwarfForm form_) : name(name_), form(form_) { }
};

template <typename Elf>
struct DwarfAbbreviation {
    intmax_t code;
    DwarfTag tag;
    enum DwarfHasChildren hasChildren;
    std::list<DwarfAttributeSpec> specs;
    DwarfAbbreviation(DWARFReader<Elf> &, intmax_t code);
    DwarfAbbreviation() {}
};

template <typename Elf>
struct DwarfPubname {
    uint32_t offset;
    std::string name;
    DwarfPubname(DWARFReader<Elf> &r, uint32_t offset);
};

struct DwarfARange {
    uintmax_t start;
    uintmax_t length;
    DwarfARange(uintmax_t start_, uintmax_t length_) : start(start_), length(length_) {}
};

template <typename Elf>
struct DwarfARangeSet {
    uint32_t length;
    uint16_t version;
    uint32_t debugInfoOffset;
    uint8_t addrlen;
    uint8_t segdesclen;
    std::vector<DwarfARange> ranges;
    DwarfARangeSet(DWARFReader<Elf> &r);
};

template <typename Elf>
struct DwarfPubnameUnit {
    uint16_t length;
    uint16_t version;
    uint32_t infoOffset;
    uint32_t infoLength;
    std::list<DwarfPubname<Elf>> pubnames;
    DwarfPubnameUnit(DWARFReader<Elf> &r);
};

struct DwarfBlock {
    off_t offset;
    off_t length;
};

union DwarfValue {
    uintmax_t addr;
    uintmax_t udata;
    intmax_t sdata;
    uintmax_t ref;
    const char *string;
    DwarfBlock block;
    char flag;
};

template <typename Elf>
struct DwarfAttribute {
    const DwarfAttributeSpec *spec; /* From abbrev table attached to type */
    const DwarfEntry<Elf> *entry;
    DwarfValue value;
    DwarfAttribute(DWARFReader<Elf> &, const DwarfEntry<Elf> *, const DwarfAttributeSpec *spec);
    ~DwarfAttribute() {
        if (spec && spec->form == DW_FORM_string)
            free((void *)(const void *)value.string);
    }
    DwarfAttribute() : spec(0), entry(0) {}
    DwarfAttribute(const DwarfAttribute &rhs) : spec(rhs.spec), entry(rhs.entry) {
        if (spec && spec->form == DW_FORM_string)
            value.string = strdup(rhs.value.string);
        else
            value.udata = rhs.value.udata;
    }
    DwarfAttribute &operator = (const DwarfAttribute &rhs) {
        entry = rhs.entry;
        if (spec && spec->form == DW_FORM_string)
            value.string = strdup(rhs.value.string);
        spec = rhs.spec;
        if (spec && spec->form == DW_FORM_string)
            value.string = strdup(rhs.value.string);
        else
            value.udata = rhs.value.udata;
        return *this;
    }
    const DwarfEntry<Elf> *getRef() const;
};

template <typename Elf>
struct DwarfEntry {
    typedef std::map<off_t, std::shared_ptr<DwarfEntry<Elf>>> ByAddr;
    ByAddr children;
    const DwarfUnit<Elf> *unit;
    const DwarfAbbreviation<Elf> *type;
    intmax_t offset;
    std::map<DwarfAttrName, DwarfAttribute<Elf>> attributes;

    DwarfEntry();

    const DwarfAttribute<Elf> &attrForName(DwarfAttrName name) const {
        auto attr = attributes.find(name);
        if (attr != attributes.end())
            return attr->second;
        throw "no such attribute";
    }

    DwarfEntry(DWARFReader<Elf> &r, intmax_t, DwarfUnit<Elf> *unit, intmax_t offset);
    const char *name() {
        try {
            return attrForName(DW_AT_name).value.string;
        }
        catch (...) {
            return "anon";
        }
    }
};

enum FIType {
    FI_DEBUG_FRAME,
    FI_EH_FRAME
};

template <typename Elf>
struct DwarfFileEntry {
    std::string name;
    std::string directory;
    unsigned lastMod;
    unsigned length;
    DwarfFileEntry(const std::string &name_, std::string dir_, unsigned lastMod_, unsigned length_);
    DwarfFileEntry(DWARFReader<Elf> &r, DwarfLineInfo<Elf> *info);
    DwarfFileEntry() {}
};

template <typename Elf>
struct DwarfLineState {
    uintmax_t addr;
    const DwarfFileEntry<Elf> *file;
    unsigned line;
    unsigned column;
    unsigned is_stmt:1;
    unsigned basic_block:1;
    unsigned end_sequence:1;
    DwarfLineState(DwarfLineInfo<Elf> *);
    void reset(DwarfLineInfo<Elf> *);
};

template <typename Elf>
struct DwarfLineInfo {
    int default_is_stmt;
    uint8_t opcode_base;
    std::vector<int> opcode_lengths;
    std::vector<std::string> directories;
    std::vector<DwarfFileEntry<Elf>> files;
    std::vector<DwarfLineState<Elf>> matrix;
    DwarfLineInfo() {}
    void build(DWARFReader<Elf> &, const DwarfUnit<Elf> *);
};

template <typename Elf>
struct DwarfUnit {
    const DwarfInfo<Elf> *dwarf;
    off_t offset;
    void decodeEntries(DWARFReader<Elf> &r, typename DwarfEntry<Elf>::ByAddr &entries);
    uint32_t length;
    uint16_t version;
    std::map<DwarfTag, DwarfAbbreviation<Elf>> abbreviations;
    uint8_t addrlen;
    const unsigned char *entryPtr;
    const unsigned char *lineInfo;
    typename DwarfEntry<Elf>::ByAddr entries;
    typename DwarfEntry<Elf>::ByAddr allEntries;
    DwarfLineInfo<Elf> lines;
    DwarfUnit(const DwarfInfo<Elf> *, DWARFReader<Elf> &);
    std::string name() const;
    DwarfUnit() : dwarf(0), offset(-1) {}
};

template <typename Elf>
struct DwarfFDE {
    DwarfCIE<Elf> *cie;
    uintmax_t iloc;
    uintmax_t irange;
    uintmax_t instructions;
    uintmax_t end;
    std::vector<unsigned char> aug;
    DwarfFDE(DwarfInfo<Elf> *, DWARFReader<Elf> &, DwarfCIE<Elf> * , uintmax_t end);
};

#define MAXREG 128
enum DwarfRegisterType {
    UNDEF,
    SAME,
    OFFSET,
    VAL_OFFSET,
    EXPRESSION,
    VAL_EXPRESSION,
    REG,
    ARCH
};

struct DwarfRegisterUnwind {
    enum DwarfRegisterType type;
    union {
        uintmax_t same;
        uintmax_t offset;
        uintmax_t reg;
        DwarfBlock expression;
        uintmax_t arch;
    } u;
};

struct DwarfCallFrame {
    DwarfRegisterUnwind registers[MAXREG];
    int cfaReg;
    DwarfRegisterUnwind cfaValue;
    DwarfCallFrame();
    // default copy constructor is valid.
};

template <typename Elf>
struct DwarfCIE {
    const DwarfInfo<Elf> *info;
    uint8_t version;
    uint8_t addressEncoding;
    unsigned char lsdaEncoding;
    bool isSignalHandler;
    unsigned codeAlign;
    int dataAlign;
    int rar;
    uintmax_t instructions;
    uintmax_t end;
    uintmax_t personality;
    unsigned long augSize;
    std::string augmentation;
    DwarfCIE(const DwarfInfo<Elf> *, DWARFReader<Elf> &, uintmax_t);
    DwarfCIE() {}
    DwarfCallFrame execInsns(DWARFReader<Elf> &r, int version, uintmax_t addr, uintmax_t wantAddr);
};

template <typename Elf>
struct DwarfFrameInfo {
    const DwarfInfo<Elf> *dwarf;
    FIType type;
    std::map<uintmax_t, DwarfCIE<Elf>> cies;
    std::list<DwarfFDE<Elf>> fdeList;
    DwarfFrameInfo(DwarfInfo<Elf> *, DWARFReader<Elf> &, FIType type);
    uintmax_t decodeCIEFDEHdr(int version, DWARFReader<Elf> &, uintmax_t &id, enum FIType, DwarfCIE<Elf> **);
    const DwarfFDE<Elf> *findFDE(uintmax_t) const;
    bool isCIE(uintmax_t id);
};

template <typename Elf>
class DwarfInfo {
    mutable std::list<DwarfPubnameUnit<Elf>> pubnameUnits;
    mutable std::list<DwarfARangeSet<Elf>> aranges;
    mutable std::map<uintmax_t, std::shared_ptr<DwarfUnit<Elf>>> unitsm;
    mutable ElfSection<Elf> info, debstr, pubnamesh, arangesh, debug_frame;
public:
    std::map<uintmax_t, DwarfCallFrame> callFrameForAddr;
    const ElfSection<Elf> abbrev, lineshdr;
    // interesting shdrs from the exe.
    std::shared_ptr<ElfObject<Elf>> elf;
    std::list<DwarfARangeSet<Elf>> &ranges() const;
    std::list<DwarfPubnameUnit<Elf>> &pubnames() const;
    std::map<uintmax_t, std::shared_ptr<DwarfUnit<Elf>>> &units() const;
    char *debugStrings;
    off_t lines;
    int version;
    std::unique_ptr<DwarfFrameInfo<Elf>> debugFrame;
    std::unique_ptr<DwarfFrameInfo<Elf>> ehFrame;
    DwarfInfo(std::shared_ptr<ElfObject<Elf>> object);
    intmax_t decodeAddress(DWARFReader<Elf> &, int encoding) const;

    std::vector<std::pair<const DwarfFileEntry<Elf> *, int>> sourceFromAddr(uintmax_t addr);

    uintmax_t unwind(Process *proc, DwarfRegisters *regs, uintmax_t addr);
    uintmax_t getCFA(const Process &proc, const DwarfCallFrame *frame, const DwarfRegisters *regs);
    ~DwarfInfo();
};

const char *dwarfSOpcodeName(enum DwarfLineSOpcode code);
const char *dwarfEOpcodeName(enum DwarfLineEOpcode code);

template <typename Elf>
int dwarfComputeCFA(Process *, const DwarfInfo<Elf> *, DwarfFDE<Elf> *, DwarfCallFrame *, DwarfRegisters *, uintmax_t addr);

void dwarfArchGetRegs(const gregset_t *regs, uintmax_t *dwarfRegs);
uintmax_t dwarfGetReg(const DwarfRegisters *regs, int regno);
void dwarfSetReg(DwarfRegisters *regs, int regno, uintmax_t regval);
DwarfRegisters *dwarfPtToDwarf(DwarfRegisters *dwarf, const CoreRegisters *sys);
const DwarfRegisters *dwarfDwarfToPt(CoreRegisters *sys, const DwarfRegisters *dwarf);

/* Linux extensions: */

enum DwarfCFAInstruction {

    DW_CFA_advance_loc          = 0x40, // XXX: Lower 6 = delta
    DW_CFA_offset               = 0x80, // XXX: lower 6 = reg, (offset:uleb128)
    DW_CFA_restore              = 0xc0, // XXX: lower 6 = register
    DW_CFA_nop                  = 0,
    DW_CFA_set_loc              = 1,    // (address)
    DW_CFA_advance_loc1         = 0x02, // (1-byte delta)
    DW_CFA_advance_loc2         = 0x03, // (2-byte delta)
    DW_CFA_advance_loc4         = 0x04, // (4-byte delta)
    DW_CFA_offset_extended      = 0x05, // ULEB128 register ULEB128 offset
    DW_CFA_restore_extended     = 0x06, // ULEB128 register
    DW_CFA_undefined            = 0x07, // ULEB128 register
    DW_CFA_same_value           = 0x08, // ULEB128 register
    DW_CFA_register             = 0x09, // ULEB128 register ULEB128 register
    DW_CFA_remember_state       = 0x0a, //
    DW_CFA_restore_state        = 0x0b, //
    DW_CFA_def_cfa              = 0x0c, // ULEB128 register ULEB128 offset
    DW_CFA_def_cfa_register     = 0x0d, // ULEB128 register
    DW_CFA_def_cfa_offset       = 0x0e, // ULEB128 offset
    DW_CFA_def_cfa_expression   = 0x0f, // BLOCK

    // DWARF 3 only {
    DW_CFA_expression           = 0x10, // ULEB128 register BLOCK
    DW_CFA_offset_extended_sf   = 0x11, // ULEB128 register SLEB128 offset
    DW_CFA_def_cfa_sf           = 0x12, // ULEB128 register SLEB128 offset
    DW_CFA_def_cfa_offset_sf    = 0x13, // SLEB128 offset
    DW_CFA_val_offset           = 0x14, // ULEB128 ULEB128
    DW_CFA_val_offset_sf        = 0x15, // ULEB128 SLEB128
    DW_CFA_val_expression       = 0x16, // ULEB128 BLOCK
    // }

    DW_CFA_lo_user              = 0x1c,
    DW_CFA_GNU_window_size      = 0x2d,
    DW_CFA_GNU_args_size        = 0x2e,
    DW_CFA_GNU_negative_offset_extended = 0x2f,
    DW_CFA_hi_user              = 0x3f,

    /*
     * Value may be this high: ensure compiler generates enough
     * padding to represent this value
     */
    DW_CFA_PAD                  = 0xff
};

template <class Elf>
class DWARFReader {
    uintmax_t off;
    uintmax_t end;
    uintmax_t getuleb128shift(int *shift, bool &isSigned);
public:
    std::shared_ptr<Reader> io;
    unsigned addrLen;
    int version;
    size_t dwarfLen; // 8 => 64-bit. 4 => 32-bit.
    std::shared_ptr<ElfObject<Elf>> elf;

    DWARFReader(std::shared_ptr<Reader> io_, int version_, uintmax_t off_, uintmax_t size_, size_t dwarfLen_)
        : off(off_)
        , end(off_ + size_)
        , io(io_)
        , addrLen(ELF_BITS / 8)
        , dwarfLen(dwarfLen_)
        , version(version_)
    {
    }

    DWARFReader(DWARFReader &rhs, uintmax_t off_, uintmax_t size_)
        : off(off_)
        , end(off_ + size_)
        , io(rhs.io)
        , addrLen(ELF_BITS / 8)
        , dwarfLen(rhs.dwarfLen)
        , version(rhs.version)
    {
    }


    DWARFReader(const ElfSection<Elf> &section, int version_, uintmax_t off_, size_t dwarfLen_)
        : off(off_ + section->sh_offset)
        , end(section->sh_offset + section->sh_size)
        , io(section.obj.io)
        , addrLen(ELF_BITS / 8)
        , dwarfLen(dwarfLen_)
        , version(version_)
    {
    }

    uint32_t getu32();
    uint16_t getu16();
    uint8_t getu8();
    int8_t gets8();
    uintmax_t getuint(int size);
    uintmax_t getfmtuint() { return getuint(dwarfLen); }
    uintmax_t getfmtint() { return getint(dwarfLen); }
    intmax_t getint(int size);
    uintmax_t getuleb128();
    intmax_t getsleb128();
    std::string getstring();
    uintmax_t getOffset() { return off; }
    uintmax_t getLimit() { return end; }
    void setOffset(uintmax_t off_) { off = off_; }
    bool empty() { return off == end; }
    uintmax_t getlength(size_t *);
    void skip(uintmax_t amount) { off += amount; }
};


#define DWARF_OP(op, value, args) op = value,

enum DwarfExpressionOp {
#include "dwarf/ops.h"
    LASTOP = 0x100
};

#undef DWARF_OP

#define DW_EH_PE_absptr 0x00
#define DW_EH_PE_uleb128        0x01
#define DW_EH_PE_udata2 0x02
#define DW_EH_PE_udata4 0x03
#define DW_EH_PE_udata8 0x04
#define DW_EH_PE_sleb128        0x09
#define DW_EH_PE_sdata2 0x0A
#define DW_EH_PE_sdata4 0x0B
#define DW_EH_PE_sdata8 0x0C

#define DW_EH_PE_pcrel  0x10
#define DW_EH_PE_textrel        0x20
#define DW_EH_PE_datarel        0x30
#define DW_EH_PE_funcrel        0x40
#define DW_EH_PE_aligned        0x50
bool dwarfUnwind(Process &, DwarfRegisters *, uintmax_t &pc /*in/out */);
#endif
