#include <utility>
#include <tuple>

#include "dataassociator.h"

DataAssociator::DataAssociator(std::unique_ptr<TrackManager>
                                trackManager,
                               std::unique_ptr<ResultComparator>
                                resultComparator,
                               std::unique_ptr<ListResultComparator>
                                listResultComparator,
                               double threshold)
  : resultComparator_(std::move(resultComparator)),
    listResultComparator_(std::move(listResultComparator)),
    trackManager_(std::move(trackManager)),
    DRRateThreshold_(threshold),
    computed_(false)
{
}

void DataAssociator::compute()
{
  if (computed_)
    return;

  const std::set<std::shared_ptr<Track> >& tracks
      = trackManager_->getTracksRef();
  for (std::shared_ptr<Track> track : tracks)
  {
    std::set<DetectionReport> DRs = getListForTrack(*track);
    associatedDRs_[track] = DRs;
  }
  // not associated DRs are stored now in DRGroups
  computed_ = true;
}

std::map<std::shared_ptr<Track>,std::set<DetectionReport> >
DataAssociator::getDRsForTracks()
{
  compute();
  return associatedDRs_;
}

std::vector<std::set<DetectionReport> >
DataAssociator::getNotAssociated()
{
  compute();
  return DRGroups_;
}

void DataAssociator::setInput(std::vector<std::set<DetectionReport> >& DRsGroups)
{
  DRGroups_ = std::move(DRsGroups);
  computed_ = false; // new data set - not yet computed
}

void DataAssociator::setDRResultComparator(std::unique_ptr<ResultComparator> comparator)
{
  resultComparator_ = std::move(comparator);
}

void DataAssociator::setListResultComparator(std::unique_ptr<ListResultComparator> comparator)
{
  listResultComparator_ = std::move(comparator);
}

void DataAssociator::setFeatureExtractor(std::unique_ptr<FeatureExtractor> extractor)
{
  featureExtractor_ = std::move(extractor);
}

void DataAssociator::setTrackManager(std::unique_ptr<TrackManager> manager)
{
  trackManager_ = std::move(manager);
}

void DataAssociator::setDRRateThreshold(double threshold)
{
  DRRateThreshold_ = threshold;
}

std::set<DetectionReport> DataAssociator::getListForTrack(const Track& track)
{
  std::tuple<
      double,
      std::set<DetectionReport>, // choosen group
      std::set<DetectionReport> // rest group (not choosen)
      > choosen(-1,std::set<DetectionReport>(),std::set<DetectionReport>());

  std::vector<std::set<DetectionReport> >::iterator it = DRGroups_.begin();
  std::vector<std::set<DetectionReport> >::const_iterator endIt
      = DRGroups_.end();

  std::vector<std::set<DetectionReport> >::iterator choosenIt
      = DRGroups_.end();

  for (; it != endIt; ++it)
  {
    std::set<DetectionReport> group = *it; // copy current group,
                                        // because rateListForTrack modifies it
    std::pair<double,std::set<DetectionReport> >
        current = rateListForTrack(group,track);

    if (current.first > std::get<0>(choosen)) // if rate is better
    {
      // current is choosen
      choosenIt = it;
      choosen = std::make_tuple(current.first,current.second,group);
    }
  }

  if (choosenIt != endIt) // if any DR was choosen
  {
    // after checking all groups
    // remove from main DR collection (DRGroups), those DRs which were choosen

    *choosenIt = std::get<2>(choosen); // overwrite choosen group, to have there only not picked DRs
  }

  return std::get<1>(choosen);
}

std::pair<double,std::set<DetectionReport> >
DataAssociator::rateListForTrack(std::set<DetectionReport>& DRs,const Track& track) const
{
  std::vector<double> rates;
  std::set<DetectionReport> result;
  auto it = DRs.begin();
  auto endIt = DRs.end();
  while (it != endIt)
  {
    double DRRate = rateDRForTrack(*it, track);
    if (DRRate >= DRRateThreshold_)
    {
      // take item from DRs set and put into result collection
      result.insert(*it);
      rates.push_back(DRRate);

      it = DRs.erase(it);
    }
    else
      ++it;
  }
  // here, all choosen detection reports were taken out of DRs collection
  // there are only not matching left.
  double overallRate = listResultComparator_->operator()(rates);
  return std::pair<double,std::set<DetectionReport> >(overallRate,result);
}

double DataAssociator::rateDRForTrack(const DetectionReport& dr, const Track& track) const
{
  ResultComparator::feature_grade_map_t m;
  for (Feature* drFeature : dr.getFeatures())
  {
    std::string name = drFeature->getName();
    Track::features_set_t trackFeatures = track.getFeatures();
    m[name] = 0;

    // TODO can be optimized - linear search vs logarithmic
    for (Feature* feature : trackFeatures)
    {
      if (feature->getName() == name)
      {
        // FIXME uncomment this, after implementing compare() method

        //double result = featureExtractor->compare(*drFeature,*feature);
        //m[name] = result; // we assume, that there are only one Feature with given name in set
      }
    }
  }

  return resultComparator_->operator()(m,dr,track);
}
