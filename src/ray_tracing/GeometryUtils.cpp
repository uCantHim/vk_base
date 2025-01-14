#include "trc/ray_tracing/GeometryUtils.h"

#include "trc/base/Device.h"

#include "trc/ray_tracing/AccelerationStructure.h"



auto trc::rt::makeGeometryInfo(const Device& device, const GeometryHandle& geo)
    -> vk::AccelerationStructureGeometryKHR
{
    return { // Array of geometries in the AS
        vk::GeometryTypeKHR::eTriangles,
        vk::AccelerationStructureGeometryDataKHR{ // a union
            vk::AccelerationStructureGeometryTrianglesDataKHR(
                vk::Format::eR32G32B32Sfloat,
                device->getBufferAddress({ geo.getVertexBuffer() }),
                geo.getVertexSize(),
                geo.getIndexCount(), // max vertex
                vk::IndexType::eUint32,
                device->getBufferAddress({ geo.getIndexBuffer() }),
                nullptr // transform data
            )
        }
    };
}

trc::rt::GeometryInstance::GeometryInstance(
    glm::mat3x4 transform,
    const BottomLevelAccelerationStructure& blas)
    :
    transform(glm::transpose(transform)),
    accelerationStructureAddress(blas.getDeviceAddress())
{
}

trc::rt::GeometryInstance::GeometryInstance(
    mat4 transform,
    const BottomLevelAccelerationStructure& blas)
    :
    transform(glm::transpose(transform)),
    accelerationStructureAddress(blas.getDeviceAddress())
{
}

trc::rt::GeometryInstance::GeometryInstance(
    glm::mat3x4 transform,
    ui32 instanceCustomIndex,
    ui8 mask,
    ui32 shaderBindingTableRecordOffset,
    vk::GeometryInstanceFlagsKHR flags,
    const BottomLevelAccelerationStructure& blas)
    :
    transform(glm::transpose(transform)),
    instanceCustomIndex(instanceCustomIndex),
    mask(mask),
    shaderBindingTableRecordOffset(shaderBindingTableRecordOffset),
    flags(static_cast<ui32>(flags)),
    accelerationStructureAddress(blas.getDeviceAddress())
{
}

trc::rt::GeometryInstance::GeometryInstance(
    mat4 transform,
    ui32 instanceCustomIndex,
    ui8 mask,
    ui32 shaderBindingTableRecordOffset,
    vk::GeometryInstanceFlagsKHR flags,
    const BottomLevelAccelerationStructure& blas)
    :
    transform(glm::transpose(transform)),
    instanceCustomIndex(instanceCustomIndex),
    mask(mask),
    shaderBindingTableRecordOffset(shaderBindingTableRecordOffset),
    flags(static_cast<ui32>(flags)),
    accelerationStructureAddress(blas.getDeviceAddress())
{
}

void trc::rt::GeometryInstance::setTransform(const mat4& t)
{
    transform = glm::transpose(t);
}
