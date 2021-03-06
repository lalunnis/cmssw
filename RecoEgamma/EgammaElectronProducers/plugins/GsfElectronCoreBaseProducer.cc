
#include "GsfElectronCoreBaseProducer.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include "DataFormats/EgammaCandidates/interface/GsfElectronCoreFwd.h"
#include "DataFormats/EgammaCandidates/interface/GsfElectronCore.h"
#include "DataFormats/ParticleFlowReco/interface/GsfPFRecTrack.h"
#include "DataFormats/GsfTrackReco/interface/GsfTrack.h"
#include "DataFormats/EgammaReco/interface/ElectronSeedFwd.h"
#include "DataFormats/EgammaReco/interface/ElectronSeed.h"
//#include "DataFormats/Common/interface/ValueMap.h"

//#include <map>

using namespace reco ;

void GsfElectronCoreBaseProducer::fillDescription( edm::ParameterSetDescription & desc )
 {
  desc.add<edm::InputTag>("gsfPfRecTracks",edm::InputTag("pfTrackElec")) ;
  desc.add<edm::InputTag>("gsfTracks",edm::InputTag("electronGsfTracks")) ;
  desc.add<edm::InputTag>("ctfTracks",edm::InputTag("generalTracks")) ;
  desc.add<bool>("useGsfPfRecTracks",true) ;
 }

GsfElectronCoreBaseProducer::GsfElectronCoreBaseProducer( const edm::ParameterSet & config )
 {
  produces<GsfElectronCoreCollection>() ;
  gsfPfRecTracksTag_ = mayConsume<reco::GsfPFRecTrackCollection>(config.getParameter<edm::InputTag>("gsfPfRecTracks")) ;
  gsfTracksTag_ = consumes<reco::GsfTrackCollection>(config.getParameter<edm::InputTag>("gsfTracks"));
  ctfTracksTag_ = consumes<reco::TrackCollection>(config.getParameter<edm::InputTag>("ctfTracks"));
  useGsfPfRecTracks_ = config.getParameter<bool>("useGsfPfRecTracks") ;
 }

GsfElectronCoreBaseProducer::~GsfElectronCoreBaseProducer()
 {}


//=======================================================================================
// For derived producers
//=======================================================================================

// to be called at the beginning of each new event
void GsfElectronCoreBaseProducer::initEvent( edm::Event & event, const edm::EventSetup & setup )
 {
  if (useGsfPfRecTracks_)
   { event.getByToken(gsfPfRecTracksTag_,gsfPfRecTracksH_) ; }
  event.getByToken(gsfTracksTag_,gsfTracksH_) ;
  event.getByToken(ctfTracksTag_,ctfTracksH_) ;
 }

void GsfElectronCoreBaseProducer::fillElectronCore( reco::GsfElectronCore * eleCore )
 {
  const GsfTrackRef & gsfTrackRef = eleCore->gsfTrack() ;

  std::pair<TrackRef,float> ctfpair = getCtfTrackRef(gsfTrackRef) ;
  eleCore->setCtfTrack(ctfpair.first,ctfpair.second) ;
 }


//=======================================================================================
// Code from Puneeth Kalavase
//=======================================================================================

std::pair<TrackRef,float> GsfElectronCoreBaseProducer::getCtfTrackRef
 ( const GsfTrackRef & gsfTrackRef )
 {
  float maxFracShared = 0;
  TrackRef ctfTrackRef = TrackRef() ;
  const TrackCollection * ctfTrackCollection = ctfTracksH_.product() ;

  // get the Hit Pattern for the gsfTrack
  const HitPattern& gsfHitPattern = gsfTrackRef->hitPattern();

  unsigned int counter ;
  TrackCollection::const_iterator ctfTkIter ;
  for ( ctfTkIter = ctfTrackCollection->begin() , counter = 0 ;
        ctfTkIter != ctfTrackCollection->end() ; ctfTkIter++, counter++ )
   {

    double dEta = gsfTrackRef->eta() - ctfTkIter->eta();
    double dPhi = gsfTrackRef->phi() - ctfTkIter->phi();
    double pi = acos(-1.);
    if(std::abs(dPhi) > pi) dPhi = 2*pi - std::abs(dPhi);

    // dont want to look at every single track in the event!
    if(sqrt(dEta*dEta + dPhi*dPhi) > 0.3) continue;

    unsigned int shared = 0 ;
    int gsfHitCounter = 0 ;
    int numGsfInnerHits = 0 ;
    int numCtfInnerHits = 0 ;
    // get the CTF Track Hit Pattern
    const HitPattern& ctfHitPattern = ctfTkIter->hitPattern() ;

    trackingRecHit_iterator elHitsIt ;
    for ( elHitsIt = gsfTrackRef->recHitsBegin() ;
          elHitsIt != gsfTrackRef->recHitsEnd() ;
          elHitsIt++, gsfHitCounter++ )
     {
      if(!((**elHitsIt).isValid()))  //count only valid Hits
       { continue ; }

      // look only in the pixels/TIB/TID
      uint32_t gsfHit = gsfHitPattern.getHitPattern(gsfHitCounter) ;
      if (!(gsfHitPattern.pixelHitFilter(gsfHit) ||
          gsfHitPattern.stripTIBHitFilter(gsfHit) ||
          gsfHitPattern.stripTIDHitFilter(gsfHit) ) )
       { continue ; }

      numGsfInnerHits++ ;

      int ctfHitsCounter = 0 ;
      numCtfInnerHits = 0 ;
      trackingRecHit_iterator ctfHitsIt ;
      for ( ctfHitsIt = ctfTkIter->recHitsBegin() ;
            ctfHitsIt != ctfTkIter->recHitsEnd() ;
            ctfHitsIt++, ctfHitsCounter++ )
       {
        if(!((**ctfHitsIt).isValid())) //count only valid Hits!
         { continue ; }

      uint32_t ctfHit = ctfHitPattern.getHitPattern(ctfHitsCounter);
      if( !(ctfHitPattern.pixelHitFilter(ctfHit) ||
            ctfHitPattern.stripTIBHitFilter(ctfHit) ||
            ctfHitPattern.stripTIDHitFilter(ctfHit) ) )
       { continue ; }

      numCtfInnerHits++ ;

        if( (**elHitsIt).sharesInput(&(**ctfHitsIt),TrackingRecHit::all) )
         {
          shared++ ;
          break ;
         }

       } //ctfHits iterator

     } //gsfHits iterator

    if ((numGsfInnerHits==0)||(numCtfInnerHits==0))
     { continue ; }

    if ( static_cast<float>(shared)/std::min(numGsfInnerHits,numCtfInnerHits) > maxFracShared )
     {
      maxFracShared = static_cast<float>(shared)/std::min(numGsfInnerHits, numCtfInnerHits);
      ctfTrackRef = TrackRef(ctfTracksH_,counter);
     }

   } //ctfTrack iterator

  return make_pair(ctfTrackRef,maxFracShared) ;
 }



