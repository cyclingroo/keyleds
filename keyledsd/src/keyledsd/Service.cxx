#include <QCoreApplication>
#include <iostream>
#include "keyledsd/Configuration.h"
#include "keyledsd/Device.h"
#include "keyledsd/Service.h"
#include "config.h"

using keyleds::Service;


Service::Service(Configuration & configuration, QObject * parent)
    : QObject(parent),
      m_configuration(configuration),
      m_deviceWatcher(nullptr, this)
{
    m_active = false;
    QObject::connect(&m_deviceWatcher, SIGNAL(deviceAdded(const device::Description &)),
                     this, SLOT(onDeviceAdded(const device::Description &)));
    QObject::connect(&m_deviceWatcher, SIGNAL(deviceRemoved(const device::Description &)),
                     this, SLOT(onDeviceRemoved(const device::Description &)));
}

Service::~Service()
{
    setActive(false);
    m_devices.clear();
}

void Service::init()
{
    setActive(true);
}

void Service::setActive(bool active)
{
    m_deviceWatcher.setActive(active);
    m_active = active;
}

void Service::onDeviceAdded(const device::Description & description)
{
    try {
        auto device = Device(description.devNode());
        auto manager = std::make_unique<DeviceManager>(
            device::Description(description), std::move(device), m_configuration
        );
        emit deviceManagerAdded(*manager);

        QObject::connect(manager.get(), SIGNAL(stopped()),
                         this, SLOT(onDeviceLoopFinished()));

        std::cout <<"Opened device " <<description.devNode()
                  <<": serial " <<manager->serial()
                  <<", model " <<manager->device().model()
                  <<" firmware " <<manager->device().firmware()
                  <<", <" <<manager->device().name() <<">" <<std::endl;

        m_devices[description.devPath()] = std::move(manager);

    } catch (Device::error & error) {
        // Suppress hid version error, it just means it's not the kind of device we want
        if (error.code() != KEYLEDS_ERROR_HIDVERSION) {
            std::cerr <<"Not opening device " <<description.devNode()
                      <<": " <<error.what() <<std::endl;
        }
    }
}

void Service::onDeviceRemoved(const device::Description & description)
{
    auto it = m_devices.find(description.devPath());
    if (it != m_devices.end()) {
        auto manager = std::move(it->second);
        m_devices.erase(it);

        std::cout <<"Removing device " <<manager->serial() <<std::endl;
        emit deviceManagerRemoved(*manager);

        if (m_devices.empty() && m_configuration.autoQuit()) {
            QCoreApplication::quit();
        }
    }
}

void Service::onDeviceLoopFinished()
{
    auto manager = static_cast<DeviceManager *>(QObject::sender());
    onDeviceRemoved(manager->description());
}
