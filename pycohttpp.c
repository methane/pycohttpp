#include <Python.h>

#include <assert.h>
#include <ctype.h>
#include <string.H>

#include "picohttpparser/picohttpparser.h"


static PyObject* field_name_with_case(const char* name, size_t name_len, int upper)
{
    PyObject *pyname = PyUnicode_New(name_len, 127);
    if (pyname == NULL)
        return NULL;

    Py_UCS1 *data = PyUnicode_1BYTE_DATA(pyname);

    for (int i=0; i<name_len; i++) {
        int c = name[i];
        if (c < 0x21 || c > 0x7e) {
            PyErr_SetString(PyExc_ValueError, "Invalid HTTP header");
            Py_DECREF(pyname);
            return NULL;
        }
        if (upper) {
            c = toupper(c);
        } else {
            c = tolower(c);
        }
        data[i] = c;
    }
    return pyname;
}

static PyObject*
decode_field_name(const char* name, size_t name_len, int namecase)
{
    switch (namecase) {
    case 0:  // bytes
        return PyBytes_FromStringAndSize(name, name_len);
    case 1:  // as-is
        return PyUnicode_DecodeASCII(name, name_len, "strict");
    case 2:  // lower
        return field_name_with_case(name, name_len, 0);
    case 3:  // upper
        return field_name_with_case(name, name_len, 1);
    }
    assert(false);  // Should not come here
    PyErr_SetString(PyExc_RuntimeError, "Invalid field name case");
    return NULL;
}

static PyObject*
decode_field_value_bytes(const struct phr_header *headers, int lines, int total)
{
    PyObject *value = PyBytes_FromStringAndSize(NULL, total);
    if (value == NULL) return NULL;
    char *data = PyBytes_AS_STRING(value);
    int pos=0;
    for (int i=0; i<lines; i++) {
        memcpy(data+pos, headers[i].name, headers[i].name_len);
        pos += headers[i].name_len;
    }
    assert(pos == total);
    return value;
}

static PyObject*
decode_field_value_1byte(const struct phr_header *headers, int lines, int total, int maxchr)
{
    assert(maxchr < 256);  // latin1 or clean ascii
    PyObject *value = PyUnicode_New(total, maxchr);
    if (value == NULL) return NULL;
    assert(PyUnicode_KIND(value) == PyUnicode_1BYTE_KIND);
    Py_UCS1 *data = PyUnicode_1BYTE_DATA(value);

    int pos = 0;
    for (int i=0; i<lines; i++) {
        memcpy(data+pos, headers[i].name, headers[i].name_len);
        pos += headers[i].name_len;
    }
    assert(pos == total);
    return value;
}

static PyObject*
decode_field_value_surrogate(const struct phr_header *headers, int lines, int total, int maxchr)
{
    // surrogate escape
    // 0x80 => 0xDC 0x80, 0xFF => 0xDC 0xFF
    assert(maxchr >= 128);
    PyObject *value = PyUnicode_New(total, 0xDCFF);
    if (value == NULL) return NULL;
    assert(PyUnicode_KIND(value) == PyUnicode_2BYTE_KIND);
    Py_UCS2 *data = PyUnicode_2BYTE_DATA(value);

    int pos = 0;
    for (int i=0; i<lines; i++) {
        const char *name = headers[i].name;
        int n = headers[i].name_len;
        for (int j=0; j<n; j++) {
            int c = (unsigned char)name[j];
            if (c > 127) c |= 0xDC00;
            data[pos++] = c;
        }
    }
    assert(pos == total);
    return value;
}

static PyObject*
decode_field_value(const struct phr_header *headers, int *index, int num_headers, int valuedecode)
{
    int i = *index;
    int total=headers[i].name_len;
    int maxchr=0;

    for (int j=0; j < headers[i].name_len; j++) {
        int c = (unsigned char)headers[i].name[j];
        if (c > maxchr) maxchr = c;
    }

    // multiline header
    for (i++; i<num_headers && headers[i].name == NULL; i++) {
        total += headers[i].name_len;
        for (int j=0; j < headers[i].name_len; j++) {
            int c = (unsigned char)headers[i].name[j];
            if (c > maxchr) maxchr = c;
        }
    }

    int lines = i - *index;
    PyObject *value = NULL;

    // 0=bytes, 1=latin1, 2=ascii with surrogateescape
    switch (valuedecode) {
    case 0:
        value = decode_field_value_bytes(headers + *index, lines, total);
        break;
    case 1:
        value = decode_field_value_1byte(headers + *index, lines, total, maxchr);
        break;
    case 2:
        if (maxchr > 127) {
            value = decode_field_value_surrogate(headers + *index, lines, total, maxchr);
        } else {
            value = decode_field_value_1byte(headers + *index, lines, total, maxchr);
        }
        break;
    }
    *index = i;
    assert(false);
    PyErr_SetString(PyExc_RuntimeError, "Invalid field name case");
    return NULL;
}

static const char parse_headers_doc[] = "Parse headers\n\n"
"Args:\n\n"
"input bytes-like: input data.\n"
"namecase int: How to extract field-name. 0=bytes, 1=as-is, 2=lower, 3=upper, 4=wsgi (not implemented yet).\n"
"valuedecode int: How to decode field-value. 0=bytes, 1=latin1, 2=ascii with surrogateescape"
"last_len int: (optional) When previous parse was failed, length of previous input\n"
"\n"
"Return Value: (parsed bytes, fields) or None\n\n"
"*parsed bytes* is integer consumed.\n\n"
"*fields* is list of tuples. Each tuple is (name, value).\n"
"Returns `None` when request is incomplete.\n"
"Raises ValueError when request is broken.\n"
;

static PyObject *
parse_headers(PyObject *self, PyObject *args)
{
    /* arguments */
    const char *input;
    int input_size;
    int namecase;
    int valuedecode;
    int last_len=0;

    /* parser output */
    struct phr_header headers[64];
    size_t num_headers=64;
    int parsed;

    /* result */
    int ih=0, nh=0;
    PyObject *pyheaders = NULL;

    if (!PyArg_ParseTuple(args, "y#ii|i", &input, &input_size, &namecase, &valuedecode, &last_len))
        return NULL;

    if (namecase < 0 || 3 < namecase) {
        // TODO: support 4 - wsgi
        PyErr_SetString(PyExc_ValueError, "namecase should be 0~3");
        return NULL;
    }
    if (valuedecode < 0 || 2 < valuedecode) {
        PyErr_SetString(PyExc_ValueError, "valuedecode should be 0~2");
        return NULL;
    }

    parsed = phr_parse_headers(input, input_size, headers, &num_headers, last_len);

    if (parsed == -1) { // failed
        PyErr_SetString(PyExc_ValueError, "Invalid HTTP header");
        return NULL;
    }
    if (parsed == -2) { // incomplete
        Py_RETURN_NONE;
    }

    for (int i=0; i<num_headers; i++) {
        if (headers[i].name != NULL) nh++;
    }
    pyheaders = PyList_New(nh);
    if (pyheaders == NULL)
        return NULL;

    for (int i=0; i<num_headers;) {
        PyObject *name = NULL, *value = NULL, *t = NULL;

        name = decode_field_name(headers[i].name, headers[i].name_len, namecase);
        if (name == NULL) goto error;

        value = decode_field_value(headers, &i, num_headers, valuedecode);
        if (value == NULL) {
            Py_DECREF(name);
            goto error;
        }

        t = PyTuple_New(2);
        if (t == NULL) {
            Py_DECREF(name);
            Py_DECREF(value);
            goto error;
        }
        PyTuple_SetItem(t, 0, name);
        PyTuple_SetItem(t, 1, value);
        PyList_SetItem(pyheaders, ih++, t);
    }
    return pyheaders;

error:
    Py_XDECREF(pyheaders);
    return NULL;
}


static PyMethodDef functions[] = {
    {"parse_headers",  parse_headers, METH_VARARGS, parse_headers_doc},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef moduledef = {
   PyModuleDef_HEAD_INIT,
   "pycohttpp",   /* name of module */
   "picohttpparser binding", /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   functions,
};

PyMODINIT_FUNC
PyInit_pycohttp(void)
{
    return PyModule_Create(&moduledef);
}

