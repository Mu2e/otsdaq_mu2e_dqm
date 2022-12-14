#ifndef _ots_DQMMu2eHistoConsumer_h_
#define _ots_DQMMu2eHistoConsumer_h_

#include "otsdaq/DataManager/DQMHistosConsumerBase.h"
#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq-dqm/ArtModules/ProtoTypeHistos.h"
#include <string>

class TFile;
class TDirectory;
class TH1F;
class TTree;

namespace ots
{

	class DQMMu2eHistoConsumer : public DQMHistosConsumerBase, public Configurable
	{
		public:
	  		DQMMu2eHistoConsumer(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& configurationPath);
			virtual ~DQMMu2eHistoConsumer(void);

			void startProcessingData (std::string runNumber) override;
			void stopProcessingData  (void) override;
			void pauseProcessingData (void) override;
			void resumeProcessingData(void) override;
			void load                (std::string fileName){;}

		private:
			bool workLoopThread(toolbox::task::WorkLoop* workLoop);
			void fastRead(void);
			void slowRead(void);
	
			//For fast read
			std::string*                       dataP_;
			std::map<std::string,std::string>* headerP_;

			bool                               saveFile_; //yes or no
			std::string                        filePath_;
			std::string                        radixFileName_;

			TH1F*                              hGauss_;//TODO need to add our own class here
			TTree*                             testTree_;
            ProtoTypeHistos                    testHistos_;
	};
}

#endif
