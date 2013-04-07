#include <chrono>
#include <thread>

#include "datamanager.h"

namespace Model
{

DataManager::DataManager(const std::string& paramsPath,
                         std::shared_ptr<DB::DynDBDriver> dynDbDriver,
                         std::unique_ptr<ReportManager> reportManager,
                         std::unique_ptr<AlignmentProcessor> alignmentProcessor,
                         std::unique_ptr<CandidateSelector> candidateSelector,
                         std::unique_ptr<DataAssociator> dataAssociator,
                         std::unique_ptr<TrackManager> trackManager,
                         std::unique_ptr<FeatureExtractor> featureExtractor,
                         std::unique_ptr<FusionExecutor> fusionExecutor,
                         std::unique_ptr<estimation::EstimationFilter<> > filter)
{
  if (dynDbDriver)
    dynDbDriver_ = dynDbDriver;
  else
    dynDbDriver_ = std::make_shared<DB::DynDBDriver>(paramsPath);

  if (reportManager)
    reportManager_ = std::move(reportManager);
  else
    reportManager_ = std::unique_ptr<ReportManager>(
          new ReportManager(dynDbDriver_)
          );

  if (alignmentProcessor)
    alignmentProcessor_ = std::move(alignmentProcessor);
  else
    alignmentProcessor_ = std::unique_ptr<AlignmentProcessor>(
          new AlignmentProcessor(boost::chrono::seconds(1)) // TODO read params from params file (options.xml)
          );

  if (candidateSelector)
    candidateSelector_ = std::move(candidateSelector);
  else
    candidateSelector_ = std::unique_ptr<CandidateSelector>(
          new CandidateSelector(dynDbDriver_)
          );

  {
    std::unique_ptr<TrackManager> tm;
    if (trackManager)
        tm = std::move(trackManager);
      else
        tm = std::unique_ptr<TrackManager>(new TrackManager(1));

    if (dataAssociator)
      dataAssociator_ = std::move(dataAssociator);
    else
    {
      std::unique_ptr<ResultComparator> resultComparator(
            new OrComparator(ResultComparator::feature_grade_map_t()));
      std::unique_ptr<ListResultComparator> listComparator(
            new OrListComparator());
      DataAssociator* da = new DataAssociator(std::move(tm),
                                              std::move(resultComparator),
                                              std::move(listComparator));
      dataAssociator_ = std::unique_ptr<DataAssociator>(da);
    }
  }

  if (trackManager)
    trackManager_ = std::move(trackManager);
  else
    trackManager_ = std::unique_ptr<TrackManager>(
          new TrackManager(0.1)
          );

  if (featureExtractor)
    featureExtractor_ = std::move(featureExtractor);
  else
    featureExtractor_ = std::unique_ptr<FeatureExtractor>(
          new FeatureExtractor()
          );

  if (fusionExecutor)
    fusionExecutor_ = std::move(fusionExecutor);
  else
    fusionExecutor_ = std::unique_ptr<FusionExecutor>(
          new FusionExecutor()
          );

  if (filter)
    filter_ = std::move(filter);
  else
  {
    estimation::KalmanFilter<>::Matrix A(4,4);
    estimation::KalmanFilter<>::Matrix B;
    estimation::KalmanFilter<>::Matrix R(2,2);
    estimation::KalmanFilter<>::Matrix Q(4,4);
    estimation::KalmanFilter<>::Matrix H(2,4);
    std::unique_ptr<estimation::EstimationFilter<> > kf(
                new estimation::KalmanFilter<>(A,B,R,Q,H)
              );
    filter_ = std::move(kf);
  }
}

Snapshot DataManager::computeState()
{
  auto tracks = computeTracks();
  // clone Tracks, to ensure safety in multithreaded environment
  Snapshot s = cloneTracksInSnapshot(tracks);
  snapshot_.put(s);
  return s;
}

Snapshot DataManager::getSnapshot() const
{
  return snapshot_.get();
}

std::shared_ptr<
      std::set<std::shared_ptr<Track> >
    >
  DataManager::computeTracks()
{
  compute(); // loops through data flow, to maintain tracking process

  // create new set of Tracks on heap
  //  and initialize it with tracks from TrackManager
  std::shared_ptr<
        std::set<std::shared_ptr<Track> >
      > tracks(
        new std::set<std::shared_ptr<Track> >(
          trackManager_->getTracks()
          )
        );

  return tracks; // return tracks to Controller/View
}

void DataManager::compute()
{
  std::set<DetectionReport> DRs = reportManager_->getDRs();
  while (!DRs.empty())
  {
    alignmentProcessor_->setDRsCollection(DRs);
    std::set<DetectionReport> alignedGroup
        = alignmentProcessor_->getNextAlignedGroup();
    while (!alignedGroup.empty())
    {
      std::vector<std::set<DetectionReport> > DRsGroups
          = candidateSelector_->getMeasurementGroups(alignedGroup);

      dataAssociator_->setInput(DRsGroups);
      std::map<std::shared_ptr<Track>,std::set<DetectionReport> > associated
          = dataAssociator_->getDRsForTracks();
      std::vector<std::set<DetectionReport> > notAssociated
          = dataAssociator_->getNotAssociated();

      std::unique_ptr<estimation::EstimationFilter<> > filter(
            filter_->clone());

      std::map<std::shared_ptr<Track>,std::set<DetectionReport> > initialized
          = trackManager_->initializeTracks(notAssociated,std::move(filter));
      // here we have Tracks:
      // associated - for these DRs which matched existing Tracks
      // initialized - for these DRs which didn't match existing Tracks
      //  (new Tracks were created)

      fusionExecutor_->fuseDRs(associated);
      fusionExecutor_->fuseDRs(initialized);

      alignedGroup = alignmentProcessor_->getNextAlignedGroup();
    }

    DRs = reportManager_->getDRs(); // get next group
  }
}

Snapshot DataManager::cloneTracksInSnapshot(std::shared_ptr<
                                             std::set<std::shared_ptr<Track> >
                                            > tracks) const
{
  std::shared_ptr<
      std::set<std::unique_ptr<Track> >
      > result(new std::set<std::unique_ptr<Track> >());
  const std::set<std::shared_ptr<Track> >& s = *tracks;
  for (auto t : s)
  { // for each track from set
    result->insert(t->clone());
  }
  return result;
}

} // namespace Model
