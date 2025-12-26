//
// Created by Lodingglue on 12/24/2025.
//

#include "classfile.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint16_t read_u2_be(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

uint32_t read_u4_be(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static cp_entry_t* parse_constant_pool(const uint8_t **ptr, uint16_t count) {
    cp_entry_t *pool = (cp_entry_t*)calloc(count, sizeof(cp_entry_t));
    if (!pool) return NULL;

    for (uint16_t i = 1; i < count; i++) {
        uint8_t tag = **ptr;
        (*ptr)++;

        size_t entry_size = 1; /* tag */

        switch (tag) {
            case CONSTANT_Utf8: {
                uint16_t length = read_u2_be(*ptr);
                entry_size += 2 + length;
                break;
            }
            case CONSTANT_Integer:
            case CONSTANT_Float:
                entry_size += 4;
                break;
            case CONSTANT_Long:
            case CONSTANT_Double:
                entry_size += 8;
                i++; /* Takes 2 slots */
                break;
            case CONSTANT_Class:
            case CONSTANT_String:
            case CONSTANT_MethodType:
                entry_size += 2;
                break;
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref:
            case CONSTANT_NameAndType:
            case CONSTANT_InvokeDynamic:
                entry_size += 4;
                break;
            case CONSTANT_MethodHandle:
                entry_size += 3;
                break;
            default:
                free(pool);
                return NULL;
        }

        pool[i].data = (uint8_t*)malloc(entry_size);
        if (!pool[i].data) {
            for (uint16_t j = 1; j < i; j++) free(pool[j].data);
            free(pool);
            return NULL;
        }

        pool[i].data[0] = tag;
        memcpy(pool[i].data + 1, *ptr, entry_size - 1);
        pool[i].size = entry_size;
        *ptr += entry_size - 1;
    }

    return pool;
}

static attribute_t* parse_attributes(const uint8_t **ptr, uint16_t count) {
    if (count == 0) return NULL;

    attribute_t *attrs = (attribute_t*)calloc(count, sizeof(attribute_t));
    if (!attrs) return NULL;

    for (uint16_t i = 0; i < count; i++) {
        attrs[i].name_index = read_u2_be(*ptr);
        *ptr += 2;
        attrs[i].length = read_u4_be(*ptr);
        *ptr += 4;

        if (attrs[i].length > 0) {
            attrs[i].info = (uint8_t*)malloc(attrs[i].length);
            if (!attrs[i].info) {
                for (uint16_t j = 0; j < i; j++) free(attrs[j].info);
                free(attrs);
                return NULL;
            }
            memcpy(attrs[i].info, *ptr, attrs[i].length);
            *ptr += attrs[i].length;
        }
    }

    return attrs;
}

static method_t* parse_methods(const uint8_t **ptr, uint16_t count) {
    if (count == 0) return NULL;

    method_t *methods = (method_t*)calloc(count, sizeof(method_t));
    if (!methods) return NULL;

    for (uint16_t i = 0; i < count; i++) {
        methods[i].access_flags = read_u2_be(*ptr);
        *ptr += 2;
        methods[i].name_index = read_u2_be(*ptr);
        *ptr += 2;
        methods[i].descriptor_index = read_u2_be(*ptr);
        *ptr += 2;
        methods[i].attributes_count = read_u2_be(*ptr);
        *ptr += 2;

        methods[i].attributes = parse_attributes(ptr, methods[i].attributes_count);
        if (methods[i].attributes_count > 0 && !methods[i].attributes) {
            for (uint16_t j = 0; j < i; j++) {
                for (uint16_t k = 0; k < methods[j].attributes_count; k++) {
                    free(methods[j].attributes[k].info);
                }
                free(methods[j].attributes);
            }
            free(methods);
            return NULL;
        }
    }

    return methods;
}

classfile_t* classfile_parse(const uint8_t *data, size_t len) {
    if (!data || len < 10) return NULL;

    classfile_t *cf = (classfile_t*)calloc(1, sizeof(classfile_t));
    if (!cf) return NULL;

    const uint8_t *ptr = data;

    /* Magic number */
    cf->magic = read_u4_be(ptr);
    ptr += 4;
    if (cf->magic != 0xCAFEBABE) {
        free(cf);
        return NULL;
    }

    /* Version */
    cf->minor_version = read_u2_be(ptr);
    ptr += 2;
    cf->major_version = read_u2_be(ptr);
    ptr += 2;

    /* Constant pool */
    cf->constant_pool_count = read_u2_be(ptr);
    ptr += 2;

    cf->constant_pool = parse_constant_pool(&ptr, cf->constant_pool_count);
    if (!cf->constant_pool) {
        free(cf);
        return NULL;
    }

    /* Access flags */
    cf->access_flags = read_u2_be(ptr);
    ptr += 2;

    /* This class, super class */
    cf->this_class = read_u2_be(ptr);
    ptr += 2;
    cf->super_class = read_u2_be(ptr);
    ptr += 2;

    /* Interfaces */
    cf->interfaces_count = read_u2_be(ptr);
    ptr += 2;

    if (cf->interfaces_count > 0) {
        cf->interfaces = (uint16_t*)malloc(cf->interfaces_count * sizeof(uint16_t));
        if (!cf->interfaces) {
            classfile_destroy(cf);
            return NULL;
        }
        for (uint16_t i = 0; i < cf->interfaces_count; i++) {
            cf->interfaces[i] = read_u2_be(ptr);
            ptr += 2;
        }
    }

    /* Fields */
    cf->fields_count = read_u2_be(ptr);
    ptr += 2;
    cf->fields = parse_methods(&ptr, cf->fields_count);

    /* Methods */
    cf->methods_count = read_u2_be(ptr);
    ptr += 2;
    cf->methods = parse_methods(&ptr, cf->methods_count);

    /* Class attributes */
    cf->attributes_count = read_u2_be(ptr);
    ptr += 2;
    cf->attributes = parse_attributes(&ptr, cf->attributes_count);

    return cf;
}

void classfile_destroy(classfile_t *cf) {
    if (!cf) return;

    /* Free constant pool */
    if (cf->constant_pool) {
        for (uint16_t i = 0; i < cf->constant_pool_count; i++) {
            free(cf->constant_pool[i].data);
        }
        free(cf->constant_pool);
    }

    /* Free interfaces */
    free(cf->interfaces);

    /* Free fields */
    if (cf->fields) {
        for (uint16_t i = 0; i < cf->fields_count; i++) {
            if (cf->fields[i].attributes) {
                for (uint16_t j = 0; j < cf->fields[i].attributes_count; j++) {
                    free(cf->fields[i].attributes[j].info);
                }
                free(cf->fields[i].attributes);
            }
        }
        free(cf->fields);
    }

    /* Free methods */
    if (cf->methods) {
        for (uint16_t i = 0; i < cf->methods_count; i++) {
            if (cf->methods[i].attributes) {
                for (uint16_t j = 0; j < cf->methods[i].attributes_count; j++) {
                    free(cf->methods[i].attributes[j].info);
                }
                free(cf->methods[i].attributes);
            }
        }
        free(cf->methods);
    }

    /* Free class attributes */
    if (cf->attributes) {
        for (uint16_t i = 0; i < cf->attributes_count; i++) {
            free(cf->attributes[i].info);
        }
        free(cf->attributes);
    }

    free(cf);
}

char* classfile_get_utf8(classfile_t *cf, uint16_t index) {
    if (!cf || index == 0 || index >= cf->constant_pool_count) return NULL;

    cp_entry_t *entry = &cf->constant_pool[index];
    if (!entry->data || entry->data[0] != CONSTANT_Utf8) return NULL;

    uint16_t length = read_u2_be(entry->data + 1);
    char *str = (char*)malloc(length + 1);
    if (!str) return NULL;

    memcpy(str, entry->data + 3, length);
    str[length] = '\0';

    return str;
}

int classfile_patch_methods(classfile_t *cf, hook_target_t *targets, size_t target_count) {
    if (!cf || !targets) return 0;

    for (uint16_t i = 0; i < cf->methods_count; i++) {
        method_t *method = &cf->methods[i];

        char *method_name = classfile_get_utf8(cf, method->name_index);
        char *method_sig = classfile_get_utf8(cf, method->descriptor_index);

        if (!method_name || !method_sig) {
            free(method_name);
            free(method_sig);
            continue;
        }

        /* Check if this method should be hooked */
        int should_hook = 0;
        for (size_t j = 0; j < target_count; j++) {
            if (strcmp(method_name, targets[j].method_name) == 0 &&
                strcmp(method_sig, targets[j].method_signature) == 0) {
                should_hook = 1;
                break;
            }
        }

        free(method_name);
        free(method_sig);

        if (!should_hook) continue;

        /* Mark as native */
        method->access_flags |= ACC_NATIVE;

        /* Remove Code attribute */
        for (uint16_t j = 0; j < method->attributes_count; j++) {
            char *attr_name = classfile_get_utf8(cf, method->attributes[j].name_index);
            if (attr_name && strcmp(attr_name, "Code") == 0) {
                free(method->attributes[j].info);

                /* Shift remaining attributes */
                for (uint16_t k = j; k < method->attributes_count - 1; k++) {
                    method->attributes[k] = method->attributes[k + 1];
                }
                method->attributes_count--;
                j--;
            }
            free(attr_name);
        }
    }

    return 1;
}

uint8_t* classfile_generate(classfile_t *cf, size_t *out_len) {
    if (!cf) return NULL;

    buffer_t *buf = buffer_create(8192);
    if (!buf) return NULL;

    /* Magic and version */
    buffer_append_u4_be(buf, cf->magic);
    buffer_append_u2_be(buf, cf->minor_version);
    buffer_append_u2_be(buf, cf->major_version);

    /* Constant pool */
    buffer_append_u2_be(buf, cf->constant_pool_count);
    for (uint16_t i = 1; i < cf->constant_pool_count; i++) {
        if (cf->constant_pool[i].data) {
            buffer_append(buf, cf->constant_pool[i].data, cf->constant_pool[i].size);
        }
    }

    /* Access flags, this/super class */
    buffer_append_u2_be(buf, cf->access_flags);
    buffer_append_u2_be(buf, cf->this_class);
    buffer_append_u2_be(buf, cf->super_class);

    /* Interfaces */
    buffer_append_u2_be(buf, cf->interfaces_count);
    for (uint16_t i = 0; i < cf->interfaces_count; i++) {
        buffer_append_u2_be(buf, cf->interfaces[i]);
    }

    /* Fields */
    buffer_append_u2_be(buf, cf->fields_count);
    for (uint16_t i = 0; i < cf->fields_count; i++) {
        buffer_append_u2_be(buf, cf->fields[i].access_flags);
        buffer_append_u2_be(buf, cf->fields[i].name_index);
        buffer_append_u2_be(buf, cf->fields[i].descriptor_index);
        buffer_append_u2_be(buf, cf->fields[i].attributes_count);

        for (uint16_t j = 0; j < cf->fields[i].attributes_count; j++) {
            buffer_append_u2_be(buf, cf->fields[i].attributes[j].name_index);
            buffer_append_u4_be(buf, cf->fields[i].attributes[j].length);
            if (cf->fields[i].attributes[j].length > 0) {
                buffer_append(buf, cf->fields[i].attributes[j].info,
                            cf->fields[i].attributes[j].length);
            }
        }
    }

    /* Methods */
    buffer_append_u2_be(buf, cf->methods_count);
    for (uint16_t i = 0; i < cf->methods_count; i++) {
        buffer_append_u2_be(buf, cf->methods[i].access_flags);
        buffer_append_u2_be(buf, cf->methods[i].name_index);
        buffer_append_u2_be(buf, cf->methods[i].descriptor_index);
        buffer_append_u2_be(buf, cf->methods[i].attributes_count);

        for (uint16_t j = 0; j < cf->methods[i].attributes_count; j++) {
            buffer_append_u2_be(buf, cf->methods[i].attributes[j].name_index);
            buffer_append_u4_be(buf, cf->methods[i].attributes[j].length);
            if (cf->methods[i].attributes[j].length > 0) {
                buffer_append(buf, cf->methods[i].attributes[j].info,
                            cf->methods[i].attributes[j].length);
            }
        }
    }

    /* Class attributes */
    buffer_append_u2_be(buf, cf->attributes_count);
    for (uint16_t i = 0; i < cf->attributes_count; i++) {
        buffer_append_u2_be(buf, cf->attributes[i].name_index);
        buffer_append_u4_be(buf, cf->attributes[i].length);
        if (cf->attributes[i].length > 0) {
            buffer_append(buf, cf->attributes[i].info, cf->attributes[i].length);
        }
    }

    uint8_t *result = buffer_detach(buf, out_len);
    buffer_destroy(buf);

    return result;
}