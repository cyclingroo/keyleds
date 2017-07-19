/* Keyleds -- Gaming keyboard tool
 * Copyright (C) 2017 Julien Hartmann, juli1.hartmann@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file
 * @brief C++ wrapper for libudev
 *
 * This wrapper presents a C++ interface for reading device information and
 * getting notifications from libudev.
 */
#ifndef TOOLS_DEVICE_WATCHER_H_20E285D9
#define TOOLS_DEVICE_WATCHER_H_20E285D9

/****************************************************************************/

#include <QObject>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class QSocketNotifier;
struct udev;
struct udev_monitor;
struct udev_enumerate;
struct udev_device;

/* We use unique_ptr to get automatic disposal of libudev structures
 * Declare the custom deleters here to ensure they are always used
 * Implement them in cxx file to prevent leakage of libudev.h into other modules
 */
namespace std {
    template<> struct default_delete<struct udev> { void operator()(struct udev *) const; };
    template<> struct default_delete<struct udev_monitor> { void operator()(struct udev_monitor *) const; };
    template<> struct default_delete<struct udev_enumerate> { void operator()(struct udev_enumerate *) const; };
    template<> struct default_delete<struct udev_device> { void operator()(struct udev_device *) const; };
}

/****************************************************************************/

namespace device {

class Error : public std::runtime_error
{
public:
    Error(const std::string & what) : std::runtime_error(what) {}
};

/** Device description
 *
 * Wraps a struct udev_device instance, which describes a single device.
 * Pre-loads all device properties and attributes for faster access, but at
 * the cost of heavier initializaiton.
 */
class Description final
{
public:
    typedef std::map<std::string, std::string> property_map;
    typedef std::vector<std::string>           tag_list;
    typedef std::map<std::string, std::string> attribute_map;
public:
                    Description(struct udev_device * device);
    explicit        Description(const Description & other);
                    Description(Description && other) = default;

    Description     parent() const;
    Description     parentWithType(const std::string & subsystem,
                                   const std::string & devtype) const;
    std::vector<Description> descendantsWithType(const std::string & subsystem) const;

    std::string     devPath() const;
    std::string     subsystem() const;
    std::string     devType() const;
    std::string     sysPath() const;
    std::string     sysName() const;
    std::string     sysNum() const;
    std::string     devNode() const;
    std::string     driver() const;
    bool            isInitialized() const;
    unsigned long long seqNum() const;
    unsigned long long usecSinceInitialized() const;

    const property_map &    properties() const { return m_properties; };
    const tag_list &        tags() const { return m_tags; };
    const attribute_map &   attributes() const { return m_attributes; };

private:
    std::unique_ptr<struct udev_device> m_device;   ///< underlying libudev device instance
    property_map    m_properties;                   ///< key-value map of libudev properties
    tag_list        m_tags;                         ///< string list of libudev tags
    attribute_map   m_attributes;                   ///< key-value map of libudev attributes
};

/****************************************************************************/

/** Device watcher and enumerator
 *
 * Actively scans or passively monitors devices through libudev. Every device
 * addition or removal detected is run through filters and emits a signal
 * if it passes them.
 *
 * Scanning is incremental: first scan will emit a deviceAdded for all devices,
 * and subsequent scans will emit a combination of deviceAdded and deviceRemoved
 * signals for matching changes. When in active mode, changes are continuously
 * monitored and signals are emitted as changes happen.
 *
 * Deleting the watcher does not emit deviceRemoved signals.
 */
class DeviceWatcher : public QObject
{
    Q_OBJECT
private:
    typedef std::unordered_map<std::string, Description> device_map;
public:
                        DeviceWatcher(struct udev * udev = nullptr, QObject *parent = nullptr);
                        ~DeviceWatcher() override;

public slots:
    void                scan();
    void                setActive(bool active);

signals:
    void                deviceAdded(const device::Description &);
    void                deviceRemoved(const device::Description &);

protected:
    virtual void        setupEnumerator(struct udev_enumerate & enumerator) const;
    virtual void        setupMonitor(struct udev_monitor & monitor) const;
    virtual bool        isVisible(const Description & dev) const;

private slots:
    void                onMonitorReady(int socket);

private:
    bool                                    m_active;       ///< If set, the watcher is monitoring
                                                            ///  device changes
    std::unique_ptr<struct udev>            m_udev;         ///< Connection to udev, or nullptr
    std::unique_ptr<struct udev_monitor>    m_monitor;      ///< Monitoring endpoint, or nullptr
    std::unique_ptr<QSocketNotifier>        m_udevNotifier; ///< Connection monitor for event loop
    device_map                              m_known;        ///< Map of device syspath to description
};

/****************************************************************************/

/** Simple filter-based device watcher
 *
 * A device watcher that filters devices based on simple rules. All rules must
 * pass for a device to be matched. Rules will not update while the watcher
 * is active.
 */
class FilteredDeviceWatcher : public DeviceWatcher
{
    Q_OBJECT
public:
            FilteredDeviceWatcher(struct udev * udev = nullptr, QObject *parent = nullptr)
                : DeviceWatcher(udev, parent) {};

    void    setSubsystem(std::string val) { m_matchSubsystem = val; }
    void    setDevType(std::string val) { m_matchDevType = val; }
    void    addProperty(std::string key, std::string val)
                { m_matchProperties[key] = val; }
    void    addTag(std::string val) { m_matchTags.push_back(val); }
    void    addAttribute(std::string key, std::string val)
                { m_matchAttributes[key] = val; }

protected:
    void    setupEnumerator(struct udev_enumerate & enumerator) const override;
    void    setupMonitor(struct udev_monitor & monitor) const override;
    bool    isVisible(const Description & dev) const override;

private:
    std::string                 m_matchSubsystem;
    std::string                 m_matchDevType;
    Description::property_map   m_matchProperties;
    Description::tag_list       m_matchTags;
    Description::attribute_map  m_matchAttributes;
};

/****************************************************************************/

};

#endif
