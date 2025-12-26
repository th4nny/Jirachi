#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <stdint.h>
#include <stddef.h>
#include "buffer.h"

/* Access flags */
#define ACC_PUBLIC      0x0001
#define ACC_PRIVATE     0x0002
#define ACC_PROTECTED   0x0004
#define ACC_STATIC      0x0008
#define ACC_FINAL       0x0010
#define ACC_NATIVE      0x0100
#define ACC_ABSTRACT    0x0400

/* Constant pool tags */
#define CONSTANT_Utf8               1
#define CONSTANT_Integer            3
#define CONSTANT_Float              4
#define CONSTANT_Long               5
#define CONSTANT_Double             6
#define CONSTANT_Class              7
#define CONSTANT_String             8
#define CONSTANT_Fieldref           9
#define CONSTANT_Methodref          10
#define CONSTANT_InterfaceMethodref 11
#define CONSTANT_NameAndType        12
#define CONSTANT_MethodHandle       15
#define CONSTANT_MethodType         16
#define CONSTANT_InvokeDynamic      18

typedef struct {
    uint8_t tag;
    uint16_t length;
    uint8_t *bytes;
} cp_utf8_t;

typedef struct {
    uint8_t tag;
    uint16_t name_index;
} cp_class_t;

typedef struct {
    uint8_t tag;
    uint16_t class_index;
    uint16_t name_and_type_index;
} cp_ref_t;

typedef struct {
    uint8_t tag;
    uint16_t name_index;
    uint16_t descriptor_index;
} cp_name_and_type_t;

typedef struct {
    uint8_t *data;
    size_t size;
} cp_entry_t;

typedef struct {
    uint16_t name_index;
    uint32_t length;
    uint8_t *info;
} attribute_t;

typedef struct {
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;
    uint16_t attributes_count;
    attribute_t *attributes;
} method_t;

typedef struct {
    uint32_t magic;
    uint16_t minor_version;
    uint16_t major_version;
    uint16_t constant_pool_count;
    cp_entry_t *constant_pool;
    uint16_t access_flags;
    uint16_t this_class;
    uint16_t super_class;
    uint16_t interfaces_count;
    uint16_t *interfaces;
    uint16_t fields_count;
    method_t *fields;
    uint16_t methods_count;
    method_t *methods;
    uint16_t attributes_count;
    attribute_t *attributes;
} classfile_t;

typedef struct {
    const char *method_name;
    const char *method_signature;
} hook_target_t;

classfile_t* classfile_parse(const uint8_t *data, size_t len);
void classfile_destroy(classfile_t *cf);
uint8_t* classfile_generate(classfile_t *cf, size_t *out_len);
int classfile_patch_methods(classfile_t *cf, hook_target_t *targets, size_t target_count);
char* classfile_get_utf8(classfile_t *cf, uint16_t index);

uint16_t read_u2_be(const uint8_t *data);
uint32_t read_u4_be(const uint8_t *data);

#endif /* CLASSFILE_H */
