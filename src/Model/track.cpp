#include "track.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp> // for logging purpose

#include "detectionreport.h"

#include <Common/logger.h>

Track::Track(std::unique_ptr<estimation::EstimationFilter<> > filter,
             double longitude, double latitude, double metersOverSea,
             double lonVar, double latVar, double mosVar,
             time_types::ptime_t creationTime)
  : estimationFilter_(std::move(filter)),
    lon_(longitude), lat_(latitude), mos_(metersOverSea),
    lonVel_(0), // because sensors don't provide information about velocity
    latVel_(0), // we assume that starting velocity is 0
    mosVel_(0), // TODO conside changing it,
    predictedLon_(0), // while Tracker's efficiency is not good enough
    predictedLat_(0),
    predictedMos_(0),
    lonPredictionVar_(0), latPredictionVar_(0), mosPredictionVar_(0),
    refreshTime_(creationTime),
    uuid_(boost::uuids::random_generator()()) // generate random uuid
{
  std::pair<
              estimation::EstimationFilter<>::vector_t,
              estimation::EstimationFilter<>::vector_t
           > prediction = initializeFilter(lon_,lat_,mos_,lonVar,latVar,mosVar);

  storePredictions(prediction);
}

void Track::refresh(time_types::ptime_t refreshTime)
{
  {
    std::stringstream msg;
    msg << "[" << uuid_ << "] Refreshing track; time = " << refreshTime;
    Common::GlobalLogger::getInstance().log("Track",msg.str());
  }
  if (refreshTime <= refreshTime_)
  {
    Common::GlobalLogger::getInstance()
        .log("Track","Refresh time earlier than already set, skipping.");
    return;
  }
  refreshTime_ = refreshTime;
}

void Track::setEstimationFilter(std::unique_ptr<estimation::EstimationFilter<> > filter)
{
  estimationFilter_ = std::move(filter);
}

Track::features_set_t Track::getFeatures() const
{
  return features_;
}

const Track::features_set_t& Track::getFeaturesRef() const
{
  return features_;
}

double Track::getLongitude() const
{
  return lon_;
}

double Track::getLatitude() const
{
  return lat_;
}

double Track::getMetersOverSea() const
{
  return mos_;
}

double Track::getLongitudeVelocity() const
{
  return lonVel_;
}

double Track::getLatitudeVelocity() const
{
  return latVel_;
}

double Track::getMetersOverSeaVelocity() const
{
  return mosVel_;
}

double Track::getPredictedLongitude() const
{
  return predictedLon_;
}

double Track::getPredictedLatitude() const
{
  return predictedLat_;
}

double Track::getPredictedMetersOverSea() const
{
  return predictedMos_;
}

double Track::getLongitudePredictionVariance() const
{
  return lonPredictionVar_;
}

double Track::getLatitudePredictionVariance() const
{
  return latPredictionVar_;
}

double Track::getMetersOverSeaPredictionVariance() const
{
  return mosPredictionVar_;
}

std::tuple<double,double,double> Track::getPredictedState() const
{
  return std::make_tuple(predictedLon_,predictedLat_,predictedMos_);
}

time_types::ptime_t Track::getRefreshTime() const
{
  return refreshTime_;
}

boost::uuids::uuid Track::getUuid() const
{
  return uuid_;
}

void Track::applyMeasurement(const DetectionReport& dr)
{
  Common::GlobalLogger& logger = Common::GlobalLogger::getInstance();
  {
    std::stringstream msg;
    msg << "[" << uuid_ << "] Applying measurement from DR: " << dr;
    logger.log("Track",msg.str());
  }
  time_types::ptime_t newRefreshTime = dr.getSensorTime();
  {
    std::stringstream msg;
    msg << "Current refresh time = " << refreshTime_;
    logger.log("Track",msg.str());
  }
  time_types::duration_t timePassed = newRefreshTime - refreshTime_;
  refresh(newRefreshTime); // refresh track
  return applyMeasurement(dr.getLongitude(),
                          dr.getLatitude(),
                          dr.getMetersOverSea(),
                          timePassed);
}

void Track::applyMeasurement(double longitude, double latitude, double mos,
                             time_types::duration_t timePassed)
{
  estimation::EstimationFilter<>::vector_t vec
      = coordsToStateVector(longitude,latitude,mos, // DRs don't provide information about velocity
                            lonVel_,latVel_,mosVel_); // so we take last calculated velocity

  std::pair<
        estimation::EstimationFilter<>::vector_t,
        estimation::EstimationFilter<>::vector_t
      > correctedState = estimationFilter_->correct(vec);
  estimation::EstimationFilter<>::vector_t trackCorrectedState
      = correctedState.first;

  double newLon = trackCorrectedState[0];
  double newLat = trackCorrectedState[1];

  if (timePassed != time_types::duration_t::zero())
  {
    lonVel_ = (newLon-lon_)/timePassed.count();
    latVel_ = (newLat-lat_)/timePassed.count();
  } // don't change velocity, when received next measurement with the same time as last one

  lon_ = newLon;
  lat_ = newLat;

  std::pair<
        estimation::EstimationFilter<>::vector_t,
        estimation::EstimationFilter<>::vector_t
      > predictedState = estimationFilter_->predict();

  storePredictions(predictedState);
}

bool Track::isTrackValid(time_types::ptime_t currentTime,
                         time_types::duration_t TTL) const
{
  if (currentTime - refreshTime_ <= TTL)
    return true;

  return false;
}

std::unique_ptr<Track> Track::clone() const
{
  std::unique_ptr<Track> track(new Track(*this));
  return track;
}

Track::Track(const Track& other)
  : estimationFilter_(std::move(other.estimationFilter_->clone())),
    lon_(other.lon_),
    lat_(other.lat_),
    mos_(other.mos_),
    lonVel_(other.lonVel_),
    latVel_(other.latVel_),
    mosVel_(other.mosVel_),
    predictedLon_(other.predictedLon_),
    predictedLat_(other.predictedLat_),
    predictedMos_(other.predictedMos_),
    lonPredictionVar_(other.lonPredictionVar_),
    latPredictionVar_(other.latPredictionVar_),
    mosPredictionVar_(other.mosPredictionVar_),
    refreshTime_(other.refreshTime_),
    uuid_(other.uuid_)
{}

estimation::EstimationFilter<>::vector_t
  Track::coordsToStateVector(double longitude,
                             double latitude,
                             double /*metersoversea*/,
                             double longitudeVelocity,
                             double latitudeVelocity,
                             double /*metersoverseaVelocity*/)
{
  // Metersoversea not yet implemented, because of too narrow state model
  // Expand model to include meters over sea value.

  estimation::EstimationFilter<>::vector_t state;
  state[0] = longitude;
  state[1] = latitude;
  state[2] = longitudeVelocity;
  state[3] = latitudeVelocity;

  return state;
}

std::pair<
          estimation::EstimationFilter<>::vector_t,
          estimation::EstimationFilter<>::vector_t
         > Track::initializeFilter(double longitude,
                                   double latitude,
                                   double metersoversea,
                                   double varLon,
                                   double varLat,
                                   double varMos)
{
  estimation::EstimationFilter<>::vector_t state
      = coordsToStateVector(longitude,latitude,metersoversea,
                            lonVel_,latVel_,mosVel_);

  estimation::EstimationFilter<>::vector_t covErr // uses the same method,
      = coordsToStateVector(varLon, // because output vector has the same layout
                            varLat,
                            varMos,
                            0,0,0); // we don't want putting anything in 3rd and 4th row

  return estimationFilter_->initialize(state,covErr);
}

void Track::storePredictions(std::pair<
                              estimation::EstimationFilter<>::vector_t,
                              estimation::EstimationFilter<>::vector_t
                             > prediction)
{
  estimation::EstimationFilter<>::vector_t trackPredictedState
      = prediction.first;

  predictedLon_ = trackPredictedState[0];
  predictedLat_ = trackPredictedState[1];

  estimation::EstimationFilter<>::vector_t trackPredictionVariance
      = prediction.second;

  lonPredictionVar_ = trackPredictionVariance[0];
  latPredictionVar_ = trackPredictionVariance[1];
}
