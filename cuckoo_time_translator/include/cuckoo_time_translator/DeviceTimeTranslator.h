#ifndef HAD8B0C21_4B6C_4EED_92B0_B8653420AD87
#define HAD8B0C21_4B6C_4EED_92B0_B8653420AD87

#include <cstdint>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <ros/common.h>
#pragma GCC diagnostic pop

#include <cuckoo_time_translator/TimestampUnwrapper.h>

namespace cuckoo_time_translator {

class DeviceTimeTranslatorConfig;

//TODO (c++11) use enum class
struct FilterAlgorithm {
  enum Type {
    None,
    ConvexHull,
    Kalman,
  } type;

  FilterAlgorithm(Type t) : type(t) {}
  bool operator == (const FilterAlgorithm & other) { return type == other.type; }
  bool operator != (const FilterAlgorithm & other) { return type != other.type; }
  // TODO (c++11) make explicit
  operator unsigned () const { return static_cast<unsigned>(type); }
};

class NS {
 public:
  // TODO (c++11) use constexpr here;
  static const char * const kDefaultSubnamespace;

  /**
   * Use nameSpace + (appendDeviceTimeSubnamespace ? "/device_time" : "") as namespace for the device time topics and parameters.
   * @param nameSpace base namespace for topics and parameters
   * @param appendDeviceTimeSubnamespace if set, append "/device_time" to nameSpace (default = true!)
   */
  NS(const std::string & nameSpace, bool appendDeviceTimeSubnamespace = true);
  NS(const char* nameSpace, bool appendDeviceTimeSubnamespace = true);

  /**
   * Use nameSpace + "/" + subNameSpace as namespace for the device time messages and parameters.
   * @param nameSpace parent namespace
   * @param subNameSpace sub namespace
   */
  NS(const std::string & nameSpace, const std::string & subNameSpace);
  NS(const char* nameSpace, const char* subNameSpace);

  operator const std::string & () const { return nameSpace_; }
 private:
  std::string nameSpace_;
};


class Defaults {
 public:
  Defaults();
  ~Defaults();
  Defaults & setFilterAlgorithm(FilterAlgorithm filterAlgorithm);
  Defaults & setSwitchTimeSecs(double secs);
 private:
  class Impl;
  const Impl & getImpl() const;
  friend class DeviceTimeTranslator;
  Impl * const pImpl_;
};

class DeviceTimeTranslator {
 public:
  /**
   * @param nameSpace used for topics and parameters
   * @param defaults defaults for the configuration parameters (filter_algo, switching_time)
   */
  DeviceTimeTranslator(const NS & nameSpace, const Defaults & defaults);

  ~DeviceTimeTranslator();

  ros::Time update(const TimestampUnwrapper & timestampUnwrapper, const ros::Time & receiveTime, double offset = 0);
  ros::Time translate(const TimestampUnwrapper & timestampUnwrapper, UnwrappedStamp unwrappedEventStamp) const;
  bool isReadyToTranslate() const;

  FilterAlgorithm getCurrentFilterAlgorithm() const;
  void setFilterAlgorithm(FilterAlgorithm filterAlgorithm);

 private:
  void configCallback(DeviceTimeTranslatorConfig &config, uint32_t level);

  class Impl;
  Impl *pImpl_;
};

template <typename Unwrapper_ = TimestampUnwrapperEventOnly>
class DeviceTimeUnwrapperAndTranslator {
 public:
  typedef Unwrapper_ Unwrapper;
  typedef typename Unwrapper::Timestamp Timestamp;
  typedef typename Unwrapper_::UnwrapperClockParameters UnwrapperClockParameters;

  /**
   * @param clockParameters parameters for the Unwrapper_
   * @param nameSpace namespace used for topics and parameters
   * @param defaults defaults used in the DeviceTimeTranslator
   */
  DeviceTimeUnwrapperAndTranslator(const UnwrapperClockParameters & clockParameters, const NS & nameSpace, const Defaults & defaults = Defaults());

  ros::Time update(Timestamp eventStamp, const ros::Time & receiveTime, double offset = 0.0);

  /**
   * Unwrap a new event's device time stamp.
   *
   * All calls to the update functions (above) and this function _must_ precisely honor the strict order of the events themselves.
   * Actually the whole method family must not be called twice for the same event or two events that have the same device timestamp.
   * Otherwise this will lead to wrongly assumed wraps of the device clock and therefore sudden acceleration of device time!
   *
   * @param eventStamp the event's stamp assigned by the device.
   * @return the translated local time
   */
  UnwrappedStamp unwrapEventStamp(Timestamp eventStamp);

  ros::Time translate(UnwrappedStamp unwrappedStamp) const;
  bool isReadyToTranslate() const;
  
  FilterAlgorithm getCurrentFilterAlgorithm() const {
    return translator.getCurrentFilterAlgorithm();
  }
  void setFilterAlgorithm(FilterAlgorithm filterAlgorithm) {
    translator.setFilterAlgorithm(filterAlgorithm);
  }
 protected:
  Unwrapper timestampUnwrapper;
  DeviceTimeTranslator translator;
};

template <typename Unwrapper_ = TimestampUnwrapperEventAndTransmit>
class DeviceTimeUnwrapperAndTranslatorWithTransmitTime : public DeviceTimeUnwrapperAndTranslator<Unwrapper_> {
 public:
  typedef Unwrapper_ Unwrapper;
  typedef typename Unwrapper_::TransmitTimestamp Timestamp;
  typedef typename Unwrapper_::UnwrapperClockParameters UnwrapperClockParameters;

  //TODO (c++11) inherit constructor
  DeviceTimeUnwrapperAndTranslatorWithTransmitTime(const UnwrapperClockParameters & clockParameters, const NS & nameSpace, const Defaults & defaults = Defaults());

  ros::Time update(Timestamp eventStamp, Timestamp transmitStamp, const ros::Time & receiveTime, double offset = 0.0);
  UnwrappedStamp unwrapTransmitStamp(Timestamp eventStamp);
};

class DefaultDeviceTimeUnwrapperAndTranslator : public DeviceTimeUnwrapperAndTranslator<> {
 public:
  DefaultDeviceTimeUnwrapperAndTranslator(const WrappingClockParameters & wrappingClockParameters, const NS & nameSpace, const Defaults & defaults = Defaults()) :
    DeviceTimeUnwrapperAndTranslator<>(wrappingClockParameters, nameSpace, defaults) {}
};

class UnwrappedDeviceTimeTranslator : public DeviceTimeUnwrapperAndTranslator<TimestampPassThrough> {
 public:
  UnwrappedDeviceTimeTranslator(ClockParameters clockParameters, const NS & nameSpace, const Defaults & defaults = Defaults()) :
    DeviceTimeUnwrapperAndTranslator<TimestampPassThrough>(clockParameters, nameSpace, defaults) {}

  ros::Time translate(TimestampPassThrough::Timestamp timestamp) const {
    return DeviceTimeUnwrapperAndTranslator<TimestampPassThrough>::translate(this->timestampUnwrapper.toUnwrapped(timestamp));
  }
};

class DefaultDeviceTimeUnwrapperAndTranslatorWithTransmitTime : public DeviceTimeUnwrapperAndTranslatorWithTransmitTime<> {
 public:
  DefaultDeviceTimeUnwrapperAndTranslatorWithTransmitTime(const WrappingClockParameters & wrappingClockParameters, const NS & nameSpace) :
    DeviceTimeUnwrapperAndTranslatorWithTransmitTime<>(wrappingClockParameters, nameSpace) {}
};

}

#endif /* HAD8B0C21_4B6C_4EED_92B0_B8653420AD87 */
