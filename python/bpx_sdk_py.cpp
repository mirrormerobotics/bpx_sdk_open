#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "bpx_sdk_config.h"
#include "bpx_sdk_version.h"
#include "joint_level_control.h"
#include "motion_level_control.h"
#include "motion_types.h"
#include "request_robot_state.h"

#include <array>
#include <cstdint>
#include <exception>
#include <memory>

namespace {

using bpx_sdk::JointLevelControl;
using bpx_sdk::MotionLevelControl;
using bpx_sdk::RequestRobotState;

struct RequestRobotStateObject {
    PyObject_HEAD
    RequestRobotState* cpp;
};

struct MotionLevelControlObject {
    PyObject_HEAD
    MotionLevelControl* cpp;
};

struct JointLevelControlObject {
    PyObject_HEAD
    JointLevelControl* cpp;
};

PyTypeObject RequestRobotStateType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject MotionLevelControlType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject JointLevelControlType = {PyVarObject_HEAD_INIT(nullptr, 0)};

RequestRobotState* state_cpp(PyObject* self) {
    if (PyObject_TypeCheck(self, &MotionLevelControlType)) {
        return reinterpret_cast<MotionLevelControlObject*>(self)->cpp;
    }
    if (PyObject_TypeCheck(self, &JointLevelControlType)) {
        return reinterpret_cast<JointLevelControlObject*>(self)->cpp;
    }
    if (PyObject_TypeCheck(self, &RequestRobotStateType)) {
        return reinterpret_cast<RequestRobotStateObject*>(self)->cpp;
    }
    PyErr_SetString(PyExc_TypeError, "expected a BPX SDK object");
    return nullptr;
}

MotionLevelControl* motion_cpp(PyObject* self) {
    if (!PyObject_TypeCheck(self, &MotionLevelControlType)) {
        PyErr_SetString(PyExc_TypeError, "expected MotionLevelControl");
        return nullptr;
    }
    return reinterpret_cast<MotionLevelControlObject*>(self)->cpp;
}

JointLevelControl* joint_cpp(PyObject* self) {
    if (!PyObject_TypeCheck(self, &JointLevelControlType)) {
        PyErr_SetString(PyExc_TypeError, "expected JointLevelControl");
        return nullptr;
    }
    return reinterpret_cast<JointLevelControlObject*>(self)->cpp;
}

PyObject* py_bool(bool value) {
    return PyBool_FromLong(value ? 1 : 0);
}

bool parse_u16(PyObject* args, const char* name, uint16_t* out) {
    PyObject* value = nullptr;
    if (!PyArg_ParseTuple(args, "O", &value)) {
        return false;
    }
    const unsigned long parsed = PyLong_AsUnsignedLong(value);
    if (PyErr_Occurred()) {
        return false;
    }
    if (parsed > 65535UL) {
        PyErr_Format(PyExc_ValueError, "%s must be in range 0..65535", name);
        return false;
    }
    *out = static_cast<uint16_t>(parsed);
    return true;
}

bool sequence_to_joint_array(PyObject* object, const char* name, std::array<float, 12>* out) {
    PyObject* seq = PySequence_Fast(object, name);
    if (!seq) {
        return false;
    }
    const Py_ssize_t size = PySequence_Fast_GET_SIZE(seq);
    if (size != 12) {
        Py_DECREF(seq);
        PyErr_Format(PyExc_ValueError, "%s must contain exactly 12 numbers", name);
        return false;
    }

    PyObject** items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < size; ++i) {
        const double value = PyFloat_AsDouble(items[i]);
        if (PyErr_Occurred()) {
            Py_DECREF(seq);
            return false;
        }
        (*out)[static_cast<size_t>(i)] = static_cast<float>(value);
    }
    Py_DECREF(seq);
    return true;
}

PyObject* float_array_to_list(const float* values, size_t size) {
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(size));
    if (!list) {
        return nullptr;
    }
    for (size_t i = 0; i < size; ++i) {
        PyObject* item = PyFloat_FromDouble(values[i]);
        if (!item) {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), item);
    }
    return list;
}

PyObject* state_float_array(PyObject* self,
                            bool (RequestRobotState::*getter)(float*) const,
                            size_t size) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    std::array<float, 12> values{};
    if (!(cpp->*getter)(values.data())) {
        Py_RETURN_NONE;
    }
    return float_array_to_list(values.data(), size);
}

PyObject* joint_float_array(PyObject* self,
                            bool (JointLevelControl::*getter)(float*) const,
                            size_t size) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    std::array<float, 12> values{};
    if (!(cpp->*getter)(values.data())) {
        Py_RETURN_NONE;
    }
    return float_array_to_list(values.data(), size);
}

PyObject* state_u8(PyObject* self, bool (RequestRobotState::*getter)(uint8_t*) const) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    uint8_t value = 0;
    if (!(cpp->*getter)(&value)) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(value);
}

PyObject* state_u32(PyObject* self, bool (RequestRobotState::*getter)(uint32_t*) const) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    uint32_t value = 0;
    if (!(cpp->*getter)(&value)) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(value);
}

PyObject* state_float(PyObject* self, bool (RequestRobotState::*getter)(float*) const) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    float value = 0.0f;
    if (!(cpp->*getter)(&value)) {
        Py_RETURN_NONE;
    }
    return PyFloat_FromDouble(value);
}

PyObject* request_new(PyTypeObject* type, PyObject*, PyObject*) {
    auto* self = reinterpret_cast<RequestRobotStateObject*>(type->tp_alloc(type, 0));
    if (self) {
        self->cpp = nullptr;
    }
    return reinterpret_cast<PyObject*>(self);
}

int request_init(RequestRobotStateObject* self, PyObject*, PyObject*) {
    try {
        delete self->cpp;
        self->cpp = new RequestRobotState();
        return 0;
    } catch (const std::exception& exc) {
        PyErr_SetString(PyExc_RuntimeError, exc.what());
        return -1;
    }
}

void request_dealloc(RequestRobotStateObject* self) {
    delete self->cpp;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* motion_new(PyTypeObject* type, PyObject*, PyObject*) {
    auto* self = reinterpret_cast<MotionLevelControlObject*>(type->tp_alloc(type, 0));
    if (self) {
        self->cpp = nullptr;
    }
    return reinterpret_cast<PyObject*>(self);
}

int motion_init(MotionLevelControlObject* self, PyObject*, PyObject*) {
    try {
        delete self->cpp;
        self->cpp = new MotionLevelControl();
        return 0;
    } catch (const std::exception& exc) {
        PyErr_SetString(PyExc_RuntimeError, exc.what());
        return -1;
    }
}

void motion_dealloc(MotionLevelControlObject* self) {
    delete self->cpp;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* joint_new(PyTypeObject* type, PyObject*, PyObject*) {
    auto* self = reinterpret_cast<JointLevelControlObject*>(type->tp_alloc(type, 0));
    if (self) {
        self->cpp = nullptr;
    }
    return reinterpret_cast<PyObject*>(self);
}

int joint_init(JointLevelControlObject* self, PyObject*, PyObject*) {
    try {
        delete self->cpp;
        self->cpp = new JointLevelControl();
        return 0;
    } catch (const std::exception& exc) {
        PyErr_SetString(PyExc_RuntimeError, exc.what());
        return -1;
    }
}

void joint_dealloc(JointLevelControlObject* self) {
    delete self->cpp;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* state_enter(PyObject* self, PyObject*) {
    Py_INCREF(self);
    return self;
}

PyObject* state_exit(PyObject* self, PyObject*) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    cpp->disconnect();
    Py_RETURN_FALSE;
}

PyObject* state_connect(PyObject* self, PyObject*) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    return py_bool(cpp->connect());
}

PyObject* state_disconnect(PyObject* self, PyObject*) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    cpp->disconnect();
    Py_RETURN_NONE;
}

PyObject* state_set_robot_ip(PyObject* self, PyObject* args) {
    RequestRobotState* cpp = state_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    const char* ip = nullptr;
    if (!PyArg_ParseTuple(args, "s", &ip)) {
        return nullptr;
    }
    cpp->setRobotIp(ip);
    Py_RETURN_NONE;
}

PyObject* state_set_robot_state_upload_port(PyObject* self, PyObject* args) {
    RequestRobotState* cpp = state_cpp(self);
    uint16_t port = 0;
    if (!cpp || !parse_u16(args, "port", &port)) {
        return nullptr;
    }
    cpp->setRobotStateUploadPort(port);
    Py_RETURN_NONE;
}

PyObject* state_set_robot_state_upload_rate(PyObject* self, PyObject* args) {
    RequestRobotState* cpp = state_cpp(self);
    uint16_t rate = 0;
    if (!cpp || !parse_u16(args, "rate_hz", &rate)) {
        return nullptr;
    }
    cpp->setRobotStateUploadRate(rate);
    Py_RETURN_NONE;
}

PyObject* state_set_tcp_local_port(PyObject* self, PyObject* args) {
    RequestRobotState* cpp = state_cpp(self);
    uint16_t port = 0;
    if (!cpp || !parse_u16(args, "port", &port)) {
        return nullptr;
    }
    cpp->setTcpLocalPort(port);
    Py_RETURN_NONE;
}

PyObject* state_set_session_id(PyObject* self, PyObject* args) {
    RequestRobotState* cpp = state_cpp(self);
    uint16_t session_id = 0;
    if (!cpp || !parse_u16(args, "session_id", &session_id)) {
        return nullptr;
    }
    cpp->setSessionId(session_id);
    Py_RETURN_NONE;
}

#define STATE_FLOAT_ARRAY_METHOD(py_name, cpp_name, count)           \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        return state_float_array(self, &RequestRobotState::cpp_name, count); \
    }

STATE_FLOAT_ARRAY_METHOD(state_get_joint_position, getJointPosition, 12)
STATE_FLOAT_ARRAY_METHOD(state_get_joint_velocity, getJointVelocity, 12)
STATE_FLOAT_ARRAY_METHOD(state_get_joint_torque, getJointTorque, 12)
STATE_FLOAT_ARRAY_METHOD(state_get_imu_rpy, getImuRpy, 3)
STATE_FLOAT_ARRAY_METHOD(state_get_imu_quat, getImuQuat, 4)
STATE_FLOAT_ARRAY_METHOD(state_get_imu_acc, getImuAcc, 3)
STATE_FLOAT_ARRAY_METHOD(state_get_imu_omega, getImuOmega, 3)
STATE_FLOAT_ARRAY_METHOD(state_get_current_velocity_body, getCurrentVelocityBody, 3)
STATE_FLOAT_ARRAY_METHOD(state_get_leg_odom, getLegOdom, 3)
STATE_FLOAT_ARRAY_METHOD(state_get_motor_temperature, getMotorTemperature, 12)
STATE_FLOAT_ARRAY_METHOD(state_get_driver_temperature, getDriverTemperature, 12)
STATE_FLOAT_ARRAY_METHOD(state_get_max_velocity, getMaxVelocity, 3)

#define STATE_U8_METHOD(py_name, cpp_name)                           \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        return state_u8(self, static_cast<bool (RequestRobotState::*)(uint8_t*) const>(&RequestRobotState::cpp_name)); \
    }

STATE_U8_METHOD(state_get_current_motion_state, getCurrentMotionState)
STATE_U8_METHOD(state_get_current_gait, getCurrentGait)
STATE_U8_METHOD(state_get_last_motion_state, getLastMotionState)
STATE_U8_METHOD(state_get_last_gait, getLastGait)
STATE_U8_METHOD(state_get_sub_gait, getSubGait)
STATE_U8_METHOD(state_get_battery_level, getBatteryLevel)

PyObject* state_get_battery_current(PyObject* self, PyObject*) {
    return state_float(self, &RequestRobotState::getBatteryCurrent);
}

#define STATE_U32_METHOD(py_name, cpp_name)                          \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        return state_u32(self, &RequestRobotState::cpp_name);        \
    }

STATE_U32_METHOD(state_get_joint_state_timestamp, getJointStateTimestamp)
STATE_U32_METHOD(state_get_imu_timestamp, getImuTimestamp)
STATE_U32_METHOD(state_get_odometry_timestamp, getOdometryTimestamp)
STATE_U32_METHOD(state_get_motion_state_timestamp, getMotionStateTimestamp)
STATE_U32_METHOD(state_get_battery_timestamp, getBatteryTimestamp)

PyObject* motion_set_motion_command_rate(PyObject* self, PyObject* args) {
    MotionLevelControl* cpp = motion_cpp(self);
    uint16_t rate = 0;
    if (!cpp || !parse_u16(args, "rate_hz", &rate)) {
        return nullptr;
    }
    cpp->setMotionCommandRate(rate);
    Py_RETURN_NONE;
}

PyObject* motion_set_velocity_control_flag(PyObject* self, PyObject* args) {
    MotionLevelControl* cpp = motion_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    PyObject* enabled = nullptr;
    if (!PyArg_ParseTuple(args, "O", &enabled)) {
        return nullptr;
    }
    const int truth = PyObject_IsTrue(enabled);
    if (truth < 0) {
        return nullptr;
    }
    cpp->setVelocityControlFlag(truth != 0);
    Py_RETURN_NONE;
}

PyObject* motion_set_zero_positions_flag(PyObject* self, PyObject*) {
    MotionLevelControl* cpp = motion_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    cpp->setZeroPositionsFlag();
    Py_RETURN_NONE;
}

#define MOTION_VOID_METHOD(py_name, cpp_name)                        \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        MotionLevelControl* cpp = motion_cpp(self);                  \
        if (!cpp) {                                                  \
            return nullptr;                                          \
        }                                                            \
        cpp->cpp_name();                                             \
        Py_RETURN_NONE;                                              \
    }

MOTION_VOID_METHOD(motion_set_walk, setWalk)
MOTION_VOID_METHOD(motion_set_running, setRunning)
MOTION_VOID_METHOD(motion_set_left_flip, setLeftFlip)
MOTION_VOID_METHOD(motion_set_right_flip, setRightFlip)
MOTION_VOID_METHOD(motion_set_bipedal, setBipedal)
MOTION_VOID_METHOD(motion_set_inv_bipedal, setInvBipedal)
MOTION_VOID_METHOD(motion_set_pronk, setPronk)
MOTION_VOID_METHOD(motion_set_pace, setPace)
MOTION_VOID_METHOD(motion_set_bound, setBound)

PyObject* motion_set_velocity(PyObject* self, PyObject* args) {
    MotionLevelControl* cpp = motion_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    float x = 0.0f;
    float y = 0.0f;
    float yaw = 0.0f;
    if (!PyArg_ParseTuple(args, "fff", &x, &y, &yaw)) {
        return nullptr;
    }
    return py_bool(cpp->setVelocity(x, y, yaw));
}

#define MOTION_BOOL_METHOD(py_name, cpp_name)                        \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        MotionLevelControl* cpp = motion_cpp(self);                  \
        if (!cpp) {                                                  \
            return nullptr;                                          \
        }                                                            \
        return py_bool(cpp->cpp_name());                             \
    }

MOTION_BOOL_METHOD(motion_set_stand_up, setStandUp)
MOTION_BOOL_METHOD(motion_set_sit_down, setSitDown)
MOTION_BOOL_METHOD(motion_set_damping, setDamping)
PyObject* motion_set_upright(PyObject*, PyObject*) {
    PyErr_SetString(PyExc_NotImplementedError,
                    "setUpright is declared in the public header but is not exported by the bundled BPX SDK shared library");
    return nullptr;
}

PyObject* joint_set_joint_command(PyObject* self, PyObject* args) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    PyObject* kp_obj = nullptr;
    PyObject* pos_obj = nullptr;
    PyObject* kd_obj = nullptr;
    PyObject* vel_obj = nullptr;
    PyObject* tff_obj = nullptr;
    if (!PyArg_ParseTuple(args, "OOOOO", &kp_obj, &pos_obj, &kd_obj, &vel_obj, &tff_obj)) {
        return nullptr;
    }

    std::array<float, 12> kp{};
    std::array<float, 12> pos{};
    std::array<float, 12> kd{};
    std::array<float, 12> vel{};
    std::array<float, 12> tff{};
    if (!sequence_to_joint_array(kp_obj, "kp", &kp) ||
        !sequence_to_joint_array(pos_obj, "pos", &pos) ||
        !sequence_to_joint_array(kd_obj, "kd", &kd) ||
        !sequence_to_joint_array(vel_obj, "vel", &vel) ||
        !sequence_to_joint_array(tff_obj, "tff", &tff)) {
        return nullptr;
    }
    return py_bool(cpp->setJointCommand(kp, pos, kd, vel, tff));
}

PyObject* joint_set_array(PyObject* self,
                          PyObject* args,
                          const char* name,
                          bool (JointLevelControl::*setter)(const std::array<float, 12>&)) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    PyObject* values_obj = nullptr;
    if (!PyArg_ParseTuple(args, "O", &values_obj)) {
        return nullptr;
    }
    std::array<float, 12> values{};
    if (!sequence_to_joint_array(values_obj, name, &values)) {
        return nullptr;
    }
    return py_bool((cpp->*setter)(values));
}

PyObject* joint_set_joint_state_upload_port(PyObject* self, PyObject* args) {
    JointLevelControl* cpp = joint_cpp(self);
    uint16_t port = 0;
    if (!cpp || !parse_u16(args, "port", &port)) {
        return nullptr;
    }
    cpp->setJointStateUploadPort(port);
    Py_RETURN_NONE;
}

PyObject* joint_set_joint_kp(PyObject* self, PyObject* args) {
    return joint_set_array(self, args, "kp", &JointLevelControl::setJointKp);
}

PyObject* joint_set_joint_position(PyObject* self, PyObject* args) {
    return joint_set_array(self, args, "pos", &JointLevelControl::setJointPosition);
}

PyObject* joint_set_joint_kd(PyObject* self, PyObject* args) {
    return joint_set_array(self, args, "kd", &JointLevelControl::setJointKd);
}

PyObject* joint_set_joint_velocity(PyObject* self, PyObject* args) {
    return joint_set_array(self, args, "vel", &JointLevelControl::setJointVelocity);
}

PyObject* joint_set_joint_torque_feed_forward(PyObject* self, PyObject* args) {
    return joint_set_array(self, args, "tff", &JointLevelControl::setJointTorqueFeedForward);
}

PyObject* joint_set_zero_joint_command(PyObject* self, PyObject*) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    return py_bool(cpp->setZeroJointCommand());
}

#define JOINT_FLOAT_ARRAY_METHOD(py_name, cpp_name, count)           \
    PyObject* py_name(PyObject* self, PyObject*) {                   \
        return joint_float_array(self, &JointLevelControl::cpp_name, count); \
    }

JOINT_FLOAT_ARRAY_METHOD(joint_get_joint_position_high_rate, getJointPositionHighRate, 12)
JOINT_FLOAT_ARRAY_METHOD(joint_get_joint_velocity_high_rate, getJointVelocityHighRate, 12)
JOINT_FLOAT_ARRAY_METHOD(joint_get_joint_torque_high_rate, getJointTorqueHighRate, 12)
JOINT_FLOAT_ARRAY_METHOD(joint_get_imu_rpy_high_rate, getImuRpyHighRate, 3)
JOINT_FLOAT_ARRAY_METHOD(joint_get_imu_quat_high_rate, getImuQuatHighRate, 4)
JOINT_FLOAT_ARRAY_METHOD(joint_get_imu_acc_high_rate, getImuAccHighRate, 3)
JOINT_FLOAT_ARRAY_METHOD(joint_get_imu_omega_high_rate, getImuOmegaHighRate, 3)

PyObject* joint_get_joint_state_timestamp_high_rate(PyObject* self, PyObject*) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    float value = 0.0f;
    if (!cpp->getJointStateTimestampHighRate(&value)) {
        Py_RETURN_NONE;
    }
    return PyFloat_FromDouble(value);
}

PyObject* joint_get_joint_state_seq_high_rate(PyObject* self, PyObject*) {
    JointLevelControl* cpp = joint_cpp(self);
    if (!cpp) {
        return nullptr;
    }
    uint32_t value = 0;
    if (!cpp->getJointStateSeqHighRate(&value)) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(value);
}

#define METHOD(name, func, flags, doc) \
    {name, reinterpret_cast<PyCFunction>(func), flags, doc}

PyMethodDef StateMethods[] = {
    {"__enter__", reinterpret_cast<PyCFunction>(state_enter), METH_NOARGS, nullptr},
    {"__exit__", reinterpret_cast<PyCFunction>(state_exit), METH_VARARGS, nullptr},
    {"connect", reinterpret_cast<PyCFunction>(state_connect), METH_NOARGS, nullptr},
    {"disconnect", reinterpret_cast<PyCFunction>(state_disconnect), METH_NOARGS, nullptr},
    METHOD("setRobotIp", state_set_robot_ip, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadPort", state_set_robot_state_upload_port, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadRate", state_set_robot_state_upload_rate, METH_VARARGS, nullptr),
    METHOD("setTcpLocalPort", state_set_tcp_local_port, METH_VARARGS, nullptr),
    METHOD("setSessionId", state_set_session_id, METH_VARARGS, nullptr),
    METHOD("getJointPosition", state_get_joint_position, METH_NOARGS, nullptr),
    METHOD("getJointVelocity", state_get_joint_velocity, METH_NOARGS, nullptr),
    METHOD("getJointTorque", state_get_joint_torque, METH_NOARGS, nullptr),
    METHOD("getImuRpy", state_get_imu_rpy, METH_NOARGS, nullptr),
    METHOD("getImuQuat", state_get_imu_quat, METH_NOARGS, nullptr),
    METHOD("getImuAcc", state_get_imu_acc, METH_NOARGS, nullptr),
    METHOD("getImuOmega", state_get_imu_omega, METH_NOARGS, nullptr),
    METHOD("getCurrentVelocityBody", state_get_current_velocity_body, METH_NOARGS, nullptr),
    METHOD("getLegOdom", state_get_leg_odom, METH_NOARGS, nullptr),
    METHOD("getMotorTemperature", state_get_motor_temperature, METH_NOARGS, nullptr),
    METHOD("getDriverTemperature", state_get_driver_temperature, METH_NOARGS, nullptr),
    METHOD("getCurrentMotionState", state_get_current_motion_state, METH_NOARGS, nullptr),
    METHOD("getCurrentGait", state_get_current_gait, METH_NOARGS, nullptr),
    METHOD("getLastMotionState", state_get_last_motion_state, METH_NOARGS, nullptr),
    METHOD("getLastGait", state_get_last_gait, METH_NOARGS, nullptr),
    METHOD("getSubGait", state_get_sub_gait, METH_NOARGS, nullptr),
    METHOD("getMaxVelocity", state_get_max_velocity, METH_NOARGS, nullptr),
    METHOD("getBatteryLevel", state_get_battery_level, METH_NOARGS, nullptr),
    METHOD("getBatteryCurrent", state_get_battery_current, METH_NOARGS, nullptr),
    METHOD("getJointStateTimestamp", state_get_joint_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getImuTimestamp", state_get_imu_timestamp, METH_NOARGS, nullptr),
    METHOD("getOdometryTimestamp", state_get_odometry_timestamp, METH_NOARGS, nullptr),
    METHOD("getMotionStateTimestamp", state_get_motion_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getBatteryTimestamp", state_get_battery_timestamp, METH_NOARGS, nullptr),
    {nullptr, nullptr, 0, nullptr},
};

PyMethodDef MotionMethods[] = {
    {"__enter__", reinterpret_cast<PyCFunction>(state_enter), METH_NOARGS, nullptr},
    {"__exit__", reinterpret_cast<PyCFunction>(state_exit), METH_VARARGS, nullptr},
    {"connect", reinterpret_cast<PyCFunction>(state_connect), METH_NOARGS, nullptr},
    {"disconnect", reinterpret_cast<PyCFunction>(state_disconnect), METH_NOARGS, nullptr},
    METHOD("setRobotIp", state_set_robot_ip, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadPort", state_set_robot_state_upload_port, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadRate", state_set_robot_state_upload_rate, METH_VARARGS, nullptr),
    METHOD("setTcpLocalPort", state_set_tcp_local_port, METH_VARARGS, nullptr),
    METHOD("setSessionId", state_set_session_id, METH_VARARGS, nullptr),
    METHOD("getJointPosition", state_get_joint_position, METH_NOARGS, nullptr),
    METHOD("getJointVelocity", state_get_joint_velocity, METH_NOARGS, nullptr),
    METHOD("getJointTorque", state_get_joint_torque, METH_NOARGS, nullptr),
    METHOD("getImuRpy", state_get_imu_rpy, METH_NOARGS, nullptr),
    METHOD("getImuQuat", state_get_imu_quat, METH_NOARGS, nullptr),
    METHOD("getImuAcc", state_get_imu_acc, METH_NOARGS, nullptr),
    METHOD("getImuOmega", state_get_imu_omega, METH_NOARGS, nullptr),
    METHOD("getCurrentVelocityBody", state_get_current_velocity_body, METH_NOARGS, nullptr),
    METHOD("getLegOdom", state_get_leg_odom, METH_NOARGS, nullptr),
    METHOD("getMotorTemperature", state_get_motor_temperature, METH_NOARGS, nullptr),
    METHOD("getDriverTemperature", state_get_driver_temperature, METH_NOARGS, nullptr),
    METHOD("getCurrentMotionState", state_get_current_motion_state, METH_NOARGS, nullptr),
    METHOD("getCurrentGait", state_get_current_gait, METH_NOARGS, nullptr),
    METHOD("getLastMotionState", state_get_last_motion_state, METH_NOARGS, nullptr),
    METHOD("getLastGait", state_get_last_gait, METH_NOARGS, nullptr),
    METHOD("getSubGait", state_get_sub_gait, METH_NOARGS, nullptr),
    METHOD("getMaxVelocity", state_get_max_velocity, METH_NOARGS, nullptr),
    METHOD("getBatteryLevel", state_get_battery_level, METH_NOARGS, nullptr),
    METHOD("getBatteryCurrent", state_get_battery_current, METH_NOARGS, nullptr),
    METHOD("getJointStateTimestamp", state_get_joint_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getImuTimestamp", state_get_imu_timestamp, METH_NOARGS, nullptr),
    METHOD("getOdometryTimestamp", state_get_odometry_timestamp, METH_NOARGS, nullptr),
    METHOD("getMotionStateTimestamp", state_get_motion_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getBatteryTimestamp", state_get_battery_timestamp, METH_NOARGS, nullptr),
    METHOD("setMotionCommandRate", motion_set_motion_command_rate, METH_VARARGS, nullptr),
    METHOD("setVelocityControlFlag", motion_set_velocity_control_flag, METH_VARARGS, nullptr),
    METHOD("setZeroPositionsFlag", motion_set_zero_positions_flag, METH_NOARGS, nullptr),
    METHOD("setWalk", motion_set_walk, METH_NOARGS, nullptr),
    METHOD("setRunning", motion_set_running, METH_NOARGS, nullptr),
    METHOD("setLeftFlip", motion_set_left_flip, METH_NOARGS, nullptr),
    METHOD("setRightFlip", motion_set_right_flip, METH_NOARGS, nullptr),
    METHOD("setBipedal", motion_set_bipedal, METH_NOARGS, nullptr),
    METHOD("setInvBipedal", motion_set_inv_bipedal, METH_NOARGS, nullptr),
    METHOD("setPronk", motion_set_pronk, METH_NOARGS, nullptr),
    METHOD("setPace", motion_set_pace, METH_NOARGS, nullptr),
    METHOD("setBound", motion_set_bound, METH_NOARGS, nullptr),
    METHOD("setVelocity", motion_set_velocity, METH_VARARGS, nullptr),
    METHOD("setStandUp", motion_set_stand_up, METH_NOARGS, nullptr),
    METHOD("setSitDown", motion_set_sit_down, METH_NOARGS, nullptr),
    METHOD("setDamping", motion_set_damping, METH_NOARGS, nullptr),
    METHOD("setUpright", motion_set_upright, METH_NOARGS, nullptr),
    {nullptr, nullptr, 0, nullptr},
};

PyMethodDef JointMethods[] = {
    {"__enter__", reinterpret_cast<PyCFunction>(state_enter), METH_NOARGS, nullptr},
    {"__exit__", reinterpret_cast<PyCFunction>(state_exit), METH_VARARGS, nullptr},
    {"connect", reinterpret_cast<PyCFunction>(state_connect), METH_NOARGS, nullptr},
    {"disconnect", reinterpret_cast<PyCFunction>(state_disconnect), METH_NOARGS, nullptr},
    METHOD("setRobotIp", state_set_robot_ip, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadPort", state_set_robot_state_upload_port, METH_VARARGS, nullptr),
    METHOD("setRobotStateUploadRate", state_set_robot_state_upload_rate, METH_VARARGS, nullptr),
    METHOD("setTcpLocalPort", state_set_tcp_local_port, METH_VARARGS, nullptr),
    METHOD("setSessionId", state_set_session_id, METH_VARARGS, nullptr),
    METHOD("getJointPosition", state_get_joint_position, METH_NOARGS, nullptr),
    METHOD("getJointVelocity", state_get_joint_velocity, METH_NOARGS, nullptr),
    METHOD("getJointTorque", state_get_joint_torque, METH_NOARGS, nullptr),
    METHOD("getImuRpy", state_get_imu_rpy, METH_NOARGS, nullptr),
    METHOD("getImuQuat", state_get_imu_quat, METH_NOARGS, nullptr),
    METHOD("getImuAcc", state_get_imu_acc, METH_NOARGS, nullptr),
    METHOD("getImuOmega", state_get_imu_omega, METH_NOARGS, nullptr),
    METHOD("getCurrentVelocityBody", state_get_current_velocity_body, METH_NOARGS, nullptr),
    METHOD("getLegOdom", state_get_leg_odom, METH_NOARGS, nullptr),
    METHOD("getMotorTemperature", state_get_motor_temperature, METH_NOARGS, nullptr),
    METHOD("getDriverTemperature", state_get_driver_temperature, METH_NOARGS, nullptr),
    METHOD("getCurrentMotionState", state_get_current_motion_state, METH_NOARGS, nullptr),
    METHOD("getCurrentGait", state_get_current_gait, METH_NOARGS, nullptr),
    METHOD("getLastMotionState", state_get_last_motion_state, METH_NOARGS, nullptr),
    METHOD("getLastGait", state_get_last_gait, METH_NOARGS, nullptr),
    METHOD("getSubGait", state_get_sub_gait, METH_NOARGS, nullptr),
    METHOD("getMaxVelocity", state_get_max_velocity, METH_NOARGS, nullptr),
    METHOD("getBatteryLevel", state_get_battery_level, METH_NOARGS, nullptr),
    METHOD("getBatteryCurrent", state_get_battery_current, METH_NOARGS, nullptr),
    METHOD("getJointStateTimestamp", state_get_joint_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getImuTimestamp", state_get_imu_timestamp, METH_NOARGS, nullptr),
    METHOD("getOdometryTimestamp", state_get_odometry_timestamp, METH_NOARGS, nullptr),
    METHOD("getMotionStateTimestamp", state_get_motion_state_timestamp, METH_NOARGS, nullptr),
    METHOD("getBatteryTimestamp", state_get_battery_timestamp, METH_NOARGS, nullptr),
    METHOD("setJointStateUploadPort", joint_set_joint_state_upload_port, METH_VARARGS, nullptr),
    METHOD("setJointCommand", joint_set_joint_command, METH_VARARGS, nullptr),
    METHOD("setJointKp", joint_set_joint_kp, METH_VARARGS, nullptr),
    METHOD("setJointPosition", joint_set_joint_position, METH_VARARGS, nullptr),
    METHOD("setJointKd", joint_set_joint_kd, METH_VARARGS, nullptr),
    METHOD("setJointVelocity", joint_set_joint_velocity, METH_VARARGS, nullptr),
    METHOD("setJointTorqueFeedForward", joint_set_joint_torque_feed_forward, METH_VARARGS, nullptr),
    METHOD("setZeroJointCommand", joint_set_zero_joint_command, METH_NOARGS, nullptr),
    METHOD("getJointPositionHighRate", joint_get_joint_position_high_rate, METH_NOARGS, nullptr),
    METHOD("getJointVelocityHighRate", joint_get_joint_velocity_high_rate, METH_NOARGS, nullptr),
    METHOD("getJointTorqueHighRate", joint_get_joint_torque_high_rate, METH_NOARGS, nullptr),
    METHOD("getImuRpyHighRate", joint_get_imu_rpy_high_rate, METH_NOARGS, nullptr),
    METHOD("getImuQuatHighRate", joint_get_imu_quat_high_rate, METH_NOARGS, nullptr),
    METHOD("getImuAccHighRate", joint_get_imu_acc_high_rate, METH_NOARGS, nullptr),
    METHOD("getImuOmegaHighRate", joint_get_imu_omega_high_rate, METH_NOARGS, nullptr),
    METHOD("getJointStateTimestampHighRate", joint_get_joint_state_timestamp_high_rate, METH_NOARGS, nullptr),
    METHOD("getJointStateSeqHighRate", joint_get_joint_state_seq_high_rate, METH_NOARGS, nullptr),
    {nullptr, nullptr, 0, nullptr},
};

int ready_type(PyTypeObject* type,
               const char* name,
               Py_ssize_t basicsize,
               destructor dealloc,
               initproc init,
               newfunc new_func,
               PyMethodDef* methods,
               const char* doc) {
    type->tp_name = name;
    type->tp_basicsize = basicsize;
    type->tp_itemsize = 0;
    type->tp_dealloc = dealloc;
    type->tp_flags = Py_TPFLAGS_DEFAULT;
    type->tp_doc = doc;
    type->tp_methods = methods;
    type->tp_init = init;
    type->tp_new = new_func;
    return PyType_Ready(type);
}

int add_int_constant(PyObject* module, const char* name, long value) {
    return PyModule_AddIntConstant(module, name, value);
}

PyModuleDef ModuleDef = {
    PyModuleDef_HEAD_INIT,
    "_bpx_sdk",
    "Python bindings for BPX SDK Open.",
    -1,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

}  // namespace

PyMODINIT_FUNC PyInit__bpx_sdk() {
    if (ready_type(&RequestRobotStateType,
                   "bpx_sdk.RequestRobotState",
                   sizeof(RequestRobotStateObject),
                   reinterpret_cast<destructor>(request_dealloc),
                   reinterpret_cast<initproc>(request_init),
                   request_new,
                   StateMethods,
                   "BPX robot state query client") < 0) {
        return nullptr;
    }
    MotionLevelControlType.tp_base = &RequestRobotStateType;
    if (ready_type(&MotionLevelControlType,
                   "bpx_sdk.MotionLevelControl",
                   sizeof(MotionLevelControlObject),
                   reinterpret_cast<destructor>(motion_dealloc),
                   reinterpret_cast<initproc>(motion_init),
                   motion_new,
                   MotionMethods,
                   "BPX motion-level control client") < 0) {
        return nullptr;
    }
    JointLevelControlType.tp_base = &RequestRobotStateType;
    if (ready_type(&JointLevelControlType,
                   "bpx_sdk.JointLevelControl",
                   sizeof(JointLevelControlObject),
                   reinterpret_cast<destructor>(joint_dealloc),
                   reinterpret_cast<initproc>(joint_init),
                   joint_new,
                   JointMethods,
                   "BPX joint-level control client") < 0) {
        return nullptr;
    }

    PyObject* module = PyModule_Create(&ModuleDef);
    if (!module) {
        return nullptr;
    }

    Py_INCREF(&RequestRobotStateType);
    Py_INCREF(&MotionLevelControlType);
    Py_INCREF(&JointLevelControlType);
    if (PyModule_AddObject(module, "RequestRobotState", reinterpret_cast<PyObject*>(&RequestRobotStateType)) < 0 ||
        PyModule_AddObject(module, "MotionLevelControl", reinterpret_cast<PyObject*>(&MotionLevelControlType)) < 0 ||
        PyModule_AddObject(module, "JointLevelControl", reinterpret_cast<PyObject*>(&JointLevelControlType)) < 0) {
        Py_DECREF(module);
        return nullptr;
    }

    PyModule_AddStringConstant(module, "__version__", BPX_SDK_PROJECT_VERSION);
    PyModule_AddStringConstant(module, "DEFAULT_SERVER_IP", bpx_sdk::DEFAULT_SERVER_IP);
    add_int_constant(module, "DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT", bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT);
    add_int_constant(module, "DEFAULT_CLIENT_JOINT_STATE_UDP_PORT", bpx_sdk::DEFAULT_CLIENT_JOINT_STATE_UDP_PORT);
    add_int_constant(module, "JOINT_COUNT", bpx_sdk::kJointCount);

    add_int_constant(module, "MOTION_STATE_LYING_DOWN", static_cast<long>(bpx_sdk::MotionState::LyingDown));
    add_int_constant(module, "MOTION_STATE_STANDING_UP", static_cast<long>(bpx_sdk::MotionState::StandingUp));
    add_int_constant(module, "MOTION_STATE_PASSIVE", static_cast<long>(bpx_sdk::MotionState::Passive));
    add_int_constant(module, "MOTION_STATE_SIT_DOWN", static_cast<long>(bpx_sdk::MotionState::SitDown));
    add_int_constant(module, "MOTION_STATE_MOTION", static_cast<long>(bpx_sdk::MotionState::Motion));

    add_int_constant(module, "MOTION_GAIT_WALK", static_cast<long>(bpx_sdk::MotionGait::Walk));
    add_int_constant(module, "MOTION_GAIT_BIPEDAL", static_cast<long>(bpx_sdk::MotionGait::Bipedal));
    add_int_constant(module, "MOTION_GAIT_FLIP", static_cast<long>(bpx_sdk::MotionGait::Flip));
    add_int_constant(module, "MOTION_GAIT_WALK_PHASE", static_cast<long>(bpx_sdk::MotionGait::WalkPhase));
    add_int_constant(module, "MOTION_GAIT_POSE_TRACKING", static_cast<long>(bpx_sdk::MotionGait::PoseTracking));
    add_int_constant(module, "MOTION_GAIT_RUNNING", static_cast<long>(bpx_sdk::MotionGait::Running));

    return module;
}
