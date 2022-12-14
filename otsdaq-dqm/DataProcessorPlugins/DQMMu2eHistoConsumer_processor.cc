#include "otsdaq-dqm/DataProcessorPlugins/DQMMu2eHistoConsumer.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include <TBufferFile.h>
#include <chrono>
#include <thread>
#include <TDirectory.h>
#include <TFile.h>
#include <TH1F.h>
#include <TTree.h>

using namespace ots;

//========================================================================================================================
	DQMMu2eHistoConsumer::DQMMu2eHistoConsumer(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID, const 		ConfigurationTree& theXDAQContextConfigTree, const std::string& configurationPath)
		: WorkLoop             (processorUID)
		, DQMHistosConsumerBase(supervisorApplicationUID, bufferUID, processorUID, LowConsumerPriority)
		, Configurable         (theXDAQContextConfigTree, configurationPath)
		, saveFile_            (theXDAQContextConfigTree.getNode(configurationPath).getNode("SaveFile").getValue<bool>())
		, filePath_            (theXDAQContextConfigTree.getNode(configurationPath).getNode("FilePath").getValue<std::string>())
		, radixFileName_       (theXDAQContextConfigTree.getNode(configurationPath).getNode("RadixFileName").getValue<std::string>())

	{
		std::cout<<"[In DQMMu2eHistoConsumer () ] Initiating ..."<<std::endl;
	}

//========================================================================================================================
	DQMMu2eHistoConsumer::~DQMMu2eHistoConsumer(void)
	{
		DQMHistosBase::closeFile();
	}
//========================================================================================================================
	void DQMMu2eHistoConsumer::startProcessingData(std::string runNumber)
		{
		  std::cout << __PRETTY_FUNCTION__ << filePath_ + "/" + radixFileName_ + "_Run" + runNumber + ".root" << std::endl;
			DQMHistosBase::openFile(filePath_ + "/" + radixFileName_ + "_Run" + runNumber + ".root");
			DQMHistosBase::myDirectory_ = DQMHistosBase::theFile_->mkdir("Mu2eHistos", "Mu2eHistos");
			DQMHistosBase::myDirectory_->cd();
			

            testHistos_.BookHistos(DQMHistosBase::myDirectory_); //pass directory 
			std::cout << __PRETTY_FUNCTION__ << "Starting!" << std::endl;
			DataConsumer::startProcessingData(runNumber);
			std::cout << __PRETTY_FUNCTION__ << "Started!" << std::endl;
		}

//========================================================================================================================
	void DQMMu2eHistoConsumer::stopProcessingData(void)
	{
		std::cout<<"[In DQMMu2eHistoConsumer () ] Stopping ..."<<std::endl;
	
		DataConsumer::stopProcessingData();
		if(saveFile_)
		{
			std::cout<<"[In DQMMu2eHistoConsumer () ] Saving ..."<<std::endl;
			DQMHistosBase::save();
		}
		closeFile();
	}

//========================================================================================================================
	void DQMMu2eHistoConsumer::pauseProcessingData(void)
	{
		std::cout<<"[In DQMMu2eHistoConsumer () ] Pausing ..."<<std::endl;
		DataConsumer::stopProcessingData();
	}

//========================================================================================================================
	void DQMMu2eHistoConsumer::resumeProcessingData(void)
	{
		std::cout<<"[In DQMMu2eHistoConsumer () ] Resuming ..."<<std::endl;
		DataConsumer::startProcessingData("");
	}

//========================================================================================================================
	bool DQMMu2eHistoConsumer::workLoopThread(toolbox::task::WorkLoop* workLoop)
	{
		// std::cout<<"[In DQMMu2eHistoConsumer () ] CallingFastRead ..."<<std::endl;
		fastRead();
		return WorkLoop::continueWorkLoop_;
	}

//========================================================================================================================
	void DQMMu2eHistoConsumer::fastRead(void)
	{
		
		if(DataConsumer::read(dataP_, headerP_) < 0)//is there something in the buffer?
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));//10
			return;
		}
		
	    TBufferFile message(TBuffer::kWrite);//prepare message
	    message.WriteBuf(dataP_->data(), dataP_->size()); //copy buffer
	    message.SetReadMode();

	    message.SetBufferOffset(2 * sizeof(UInt_t));//move pointer
	    TClass *objectClass = TClass::Load(message);//load and find class
	    message.SetBufferOffset(0);//rewind buffer
	    __CFG_COUT__ << "Class name: " << objectClass->GetName() << std::endl;

	    TObject *readObject = nullptr;
	    if (objectClass->InheritsFrom(TH1::Class()))
	    {
		    __CFG_COUT__ << "TH1 class: " << objectClass->GetName() << std::endl;
		   
		    readObject = (TH1*) message.ReadObject(objectClass);///read object
		    TH1* object = (TH1*)DQMHistosBase::myDirectory_->FindObjectAny(readObject->GetName());//find in memory
		    object->Reset();
		    object->Add((TH1*)readObject);//add the filled copy
		  
		    __CFG_COUT__ << "Histo name: " << testHistos_.Test._FirstHist->GetName() << std::endl;
		    
	    }
		DataConsumer::setReadSubBuffer<std::string, std::map<std::string, std::string>>();
	}

DEFINE_OTS_PROCESSOR(DQMMu2eHistoConsumer)
