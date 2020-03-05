//
// Created by wugaojun on 11/30/18.
//
#include <Python.h>
#include <iomanip>
#include "signup_tool.h"

uint32_t generate_large_num()
{
    return 0xf0000000fffffff9;
}

//  1) Wrapper Function that returns Python object
PyObject * generate_large_num_wrapper(PyObject * self, PyObject * args)
{
    std::string account_json;
    PyObject * ret;

    // build the resulting string into a Python object.
    uint32_t num = generate_large_num();
    ret = Py_BuildValue("I", num);

    return ret;
}

PyObject * generate_account_wrapper(PyObject * self, PyObject * args)
{
    std::string account_json;
    PyObject * ret;

    // build the resulting string into a Python object.
    account_json= generate_account();
    ret = Py_BuildValue("s", account_json.c_str());

    return ret;
}

PyObject * generate_client_nonce_wrapper(PyObject * self, PyObject * args)
{
    PyObject * result;
    uint32_t difficulty;
    uint32_t serverNonce;
    uint32_t clientNonce;

    char * uid;
    if (!PyArg_ParseTuple(args, "sII", &uid, &difficulty, &serverNonce)) {
        return NULL;
    }
    //std::cout << "in nonce wrapper: " << std::hex << difficulty << " " << serverNonce << std::endl;

    // build the resulting string into a Python object.
    clientNonce = generate_client_nonce(uid, difficulty, serverNonce);
    result = Py_BuildValue("I", clientNonce);

    return result;
}

// 2) Python module
static PyMethodDef SignupMethods[] =
{
    { "generate_account", generate_account_wrapper, METH_VARARGS, "generate account" },
    { "generate_client_nonce", generate_client_nonce_wrapper, METH_VARARGS, "generate client nonce" },
    { "generate_large_num", generate_large_num_wrapper, METH_VARARGS, "generate large num" },
    { NULL, NULL, 0, NULL }
};

// 3) Module init function
static struct PyModuleDef signup_module = {
    PyModuleDef_HEAD_INIT,
    "signup",
    NULL,
    -1,
    SignupMethods
};

// 4) Module initialization function 
PyMODINIT_FUNC
PyInit_signup(void)
{
    return PyModule_Create(&signup_module);
}

