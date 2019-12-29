#include <common/bit-operations.h>
#include <c/persistent-storage.h>

struct maybe_sum {
    PersistentAccess access;
    PersistentChecksum value;
};

static inline size_t
checksum_size(const PersistentStorage *store)
{
    switch (store->checksum.type) {
    case PERSISTENT_CHECKSUM_16BIT:
        return 2u;
    case PERSISTENT_CHECKSUM_32BIT: /* FALLTHROUGH */
    default:
        return 4u;
    }
}

static inline void
set_data_address(PersistentStorage *store)
{
    store->data.address = store->checksum.address + checksum_size(store);
}

static uint16_t
trivialsum(const unsigned char *data, size_t n, uint16_t init)
{
    for (size_t i = 0u; i < n; ++i) {
        init += data[i] & 0xffu;
    }

    return init;
}

void
persistent_sum16(PersistentStorage *store, PersistentChksum16 f, uint16_t init)
{
    store->checksum.initial.sum16 = init;
    store->checksum.type = PERSISTENT_CHECKSUM_16BIT;
    store->checksum.process.c16 = f;
    set_data_address(store);
}

void
persistent_sum32(PersistentStorage *store, PersistentChksum32 f, uint32_t init)
{
    store->checksum.initial.sum32 = init;
    store->checksum.type = PERSISTENT_CHECKSUM_32BIT;
    store->checksum.process.c32 = f;
    set_data_address(store);
}

void
persistent_init(PersistentStorage *store,
                const size_t size,
                const PersistentBlockRead rd,
                const PersistentBlockWrite wr)
{
    /* Initialise checksum first; data init needs address and type of it. The
     * initialisation order of the rest doesn't really matter. */
    store->checksum.address = 0u;
    persistent_sum16(store, trivialsum, 0u);

    set_data_address(store);
    store->data.size = size;

    store->block.read = rd;
    store->block.write = wr;

    store->buffer.size = 1;
    store->buffer.data = NULL;
}

static PersistentChecksum
persistent_checksum(const PersistentStorage *store, const void *src)
{
    const unsigned char *from = src;
    PersistentChecksum rv;

    switch (store->checksum.type) {
    case PERSISTENT_CHECKSUM_16BIT:
        rv.sum16 = store->checksum.process.c16(
            from, store->data.size, store->checksum.initial.sum16);
        break;
    case PERSISTENT_CHECKSUM_32BIT: /* FALLTHROUGH */
    default:
        rv.sum32 = store->checksum.process.c32(
            from, store->data.size, store->checksum.initial.sum32);
        break;
    }

    return rv;
}

void
persistent_place(PersistentStorage *store, uint32_t address)
{
    store->checksum.address = address;
    set_data_address(store);
}

void
persistent_buffer(PersistentStorage *store, unsigned char *buffer, size_t n)
{
    store->buffer.data = buffer;
    store->buffer.size = n;
}

static struct maybe_sum
persistent_calculate_checksum(PersistentStorage *store)
{
    struct maybe_sum rv;
    unsigned char buf;
    unsigned char *data;
    size_t bsize;

    rv.value = store->checksum.initial;
    rv.access = PERSISTENT_ACCESS_IO_ERROR;

    if (store->buffer.data != NULL) {
        data = store->buffer.data;
        bsize = store->buffer.size;
    } else {
        data = &buf;
        bsize = 1u;
    }

    size_t rest = store->data.size;
    uint32_t address = store->data.address;

    while (rest > 0u) {
        const size_t toget = (rest > bsize) ? bsize : rest;
        const size_t n = store->block.read(data, address, toget);

        if (n != toget) {
            return rv;
        }

        switch (store->checksum.type) {
        case PERSISTENT_CHECKSUM_16BIT:
            rv.value.sum16 = store->checksum.process.c16(data, bsize,
                                                         rv.value.sum16);
            break;
        case PERSISTENT_CHECKSUM_32BIT: /* FALLTHROUGH */
        default:
            rv.value.sum32 = store->checksum.process.c32(data, bsize,
                                                         rv.value.sum32);
            break;
        }

        rest -= toget;
        address += toget;
    }

    rv.access = PERSISTENT_ACCESS_SUCCESS;
    return rv;
}

static PersistentAccess
persistent_store_checksum(PersistentStorage *store, PersistentChecksum sum)
{
    unsigned char data[4u];
    const size_t size = checksum_size(store);

    const uint32_t value =
        (store->checksum.type == PERSISTENT_CHECKSUM_32BIT)
        ? sum.sum32 : sum.sum16;

    for (size_t i = 0u; i < size; ++i) {
        const unsigned int shift = 8u * i;
        const uint32_t mask = 0xffull << shift;
        data[i] = ((value & mask) >> shift) & 0xffu;
    }

    const size_t n = store->block.write(store->checksum.address, data, size);
    if (n != size) {
        return PERSISTENT_ACCESS_IO_ERROR;
    }

    return PERSISTENT_ACCESS_SUCCESS;
}

static struct maybe_sum
persistent_current_sum16(PersistentStorage *store)
{
    unsigned char data[2u];
    struct maybe_sum rv;
    rv.access = PERSISTENT_ACCESS_IO_ERROR;
    const size_t n = store->block.read(data, store->checksum.address, 2u);
    if (n != 2u) {
        return rv;
    }
    rv.access = PERSISTENT_ACCESS_SUCCESS;
    rv.value.sum16 = (((data[1] & 0xffu) << 8u) | (data[0] & 0xffu));
    return rv;
}

static struct maybe_sum
persistent_current_sum32(PersistentStorage *store)
{
    unsigned char data[4u];
    struct maybe_sum rv;
    rv.access = PERSISTENT_ACCESS_IO_ERROR;
    const size_t n = store->block.read(data, store->checksum.address, 4u);
    if (n != 4u) {
        return rv;
    }
    rv.access = PERSISTENT_ACCESS_SUCCESS;
    rv.value.sum16 = ( ((data[3] & 0xffu) << 24u)
                     | ((data[2] & 0xffu) << 16u)
                     | ((data[1] & 0xffu) << 8u)
                     |  (data[0] & 0xffu));
    return rv;
}

static struct maybe_sum
persistent_current_checksum(PersistentStorage *store)
{
    switch (store->checksum.type) {
    case PERSISTENT_CHECKSUM_16BIT:
        return persistent_current_sum16(store);
    case PERSISTENT_CHECKSUM_32BIT: /* FALLTHROUGH */
    default:
        return persistent_current_sum32(store);
    }
}

static bool
persistent_match(const PersistentStorage *store,
                 PersistentChecksum a, PersistentChecksum b)
{
    switch (store->checksum.type) {
    case PERSISTENT_CHECKSUM_16BIT:
        return a.sum16 == b.sum16;
    case PERSISTENT_CHECKSUM_32BIT: /* FALLTHROUGH */
    default:
        return a.sum32 == b.sum32;
    }
}

PersistentAccess
persistent_validate(PersistentStorage *store)
{
    const struct maybe_sum stored = persistent_current_checksum(store);
    if (stored.access != PERSISTENT_ACCESS_SUCCESS) {
        return stored.access;
    }
    const struct maybe_sum calculated = persistent_calculate_checksum(store);
    if (calculated.access != PERSISTENT_ACCESS_SUCCESS) {
        return calculated.access;
    }
    const bool valid = persistent_match(store, stored.value, calculated.value);
    return valid ? PERSISTENT_ACCESS_SUCCESS : PERSISTENT_ACCESS_INVALID_DATA;
}

PersistentAccess
persistent_fetch_part(void *dst, PersistentStorage *store,
                      size_t offset, size_t n)
{
    if ((offset + n) > store->data.size) {
        return PERSISTENT_ACCESS_ADDRESS_OUT_OF_RANGE;
    }

    const uint32_t address = store->data.address + offset;
    const size_t read = store->block.read(dst, address, n);
    return (read == n) ? PERSISTENT_ACCESS_SUCCESS : PERSISTENT_ACCESS_IO_ERROR;
}

PersistentAccess
persistent_fetch(void *dst, PersistentStorage *store)
{
    return persistent_fetch_part(dst, store, 0, store->data.size);
}

PersistentAccess
persistent_store_part(PersistentStorage *store, const void *src,
                      size_t offset, size_t n)
{
    if ((offset + n) > store->data.size) {
        return PERSISTENT_ACCESS_ADDRESS_OUT_OF_RANGE;
    }

    const uint32_t address = store->data.address + offset;
    const size_t stored = store->block.write(address, src, n);
    if (stored != n) {
        return PERSISTENT_ACCESS_IO_ERROR;
    }

    PersistentChecksum sum;
    if ((offset == 0) && (n == store->data.size)) {
        sum = persistent_checksum(store, src);
    } else {
        struct maybe_sum tmp = persistent_current_checksum(store);
        if (tmp.access != PERSISTENT_ACCESS_SUCCESS) {
            return tmp.access;
        }
        sum = tmp.value;
    }

    return persistent_store_checksum(store, sum);
}

PersistentAccess
persistent_store(PersistentStorage *store, const void *src)
{
    return persistent_store_part(store, src, 0, store->data.size);
}
