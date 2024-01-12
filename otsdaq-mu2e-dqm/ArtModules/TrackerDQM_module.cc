///////////////////////////////////////////////////////////////////////////////
// Author: E. Croft, adapted from code by S. Middleton
// This module (should) produce histograms of data from the straw tracker
///////////////////////////////////////////////////////////////////////////////
#include "TRACE/tracemf.h"
#define  TRACE_NAME "TrackerDQM"

#include "TSystem.h"

#include "otsdaq-mu2e-dqm/ArtModules/TrackerDQM_module.hh"

//-----------------------------------------------------------------------------
unsigned int reverseBits(unsigned int num) {
  unsigned int numOfBits = 10; // sizeof(num) * 8; // Number of bits in an unsigned int

  unsigned int reversedNum = 0;
  for (unsigned int i = 0; i < numOfBits; ++i) {
    if ((num & (1 << i)) != 0)
      reversedNum |= 1 << ((numOfBits - 1) - i);
  }

  return reversedNum;
}

//-----------------------------------------------------------------------------
unsigned int correctedTDC(unsigned int TDC) {
  uint32_t corrected_tdc = ((TDC & 0xFFFF00) + (0xFF  - (TDC & 0xFF)));
  return corrected_tdc;
}

//-----------------------------------------------------------------------------
// NWords : the number of short words
//-----------------------------------------------------------------------------
void TrackerDQM::printFragment(const artdaq::Fragment* Frag, int NWords) {
//-----------------------------------------------------------------------------
// print fragments in HEX, for the tracker, the data has to be in 2-byte words
//-----------------------------------------------------------------------------
  ushort* buf = (ushort*) (Frag->dataBegin());

  int loc     = 0;
      
  for (int i=0; i<NWords; i++) {
    if (loc == 0) printf(" 0x%08x: ",i*2);

    ushort  word = buf[i];
    printf("0x%04x ",word);
    
    loc += 1;
    if (loc == 8) {
      printf("\n");
      loc = 0;
    }
  }
      
  if (loc != 0) printf("\n");
}

//-----------------------------------------------------------------------------
TrackerDQM::TrackerDQM(art::EDAnalyzer::Table<Config> const& conf) : 
  art::EDAnalyzer   (conf),

  _diagLevel       (conf().diagLevel       ()), 
  _minNBytes       (conf().minNBytes       ()), 
  _maxNBytes       (conf().maxNBytes       ()), 
  _dataHeaderOffset(conf().dataHeaderOffset()), 
  _activeLinks     (conf().activeLinks     ()),
  _refChCal        (conf().refChCal        ()),
  _refChHV         (conf().refChHV         ()),
  _dumpDTCRegisters(conf().dumpDTCRegisters()),
  _analyzeFragments(conf().analyzeFragments()),
  _maxFragmentSize (conf().maxFragmentSize ()),
  _pulserFrequency (conf().pulserFrequency ()),
  _port            (conf().port            ())
{
  //  conf_             (conf()),
  //  histType_         (conf().histType ()),
  //diagLevel_        (conf().diag     ()),

//-----------------------------------------------------------------------------
// for now, assume only one station, but implement data structures handling 
// full tracker
//-----------------------------------------------------------------------------
  _station = 0;
  _plane   = 0;

  double f0(31.29e6);                   // 31.29 MHz

  _timeWindow = conf().timeWindow()*25.;  // in ns

  if      (_pulserFrequency ==  60) _freq = f0/(pow(2,9)+1);     // ~ 60 kHz
  else if (_pulserFrequency == 250) _freq = f0/(pow(2,7)+1);     // ~250 kHz

  _dt   = 1/_freq*1.e9;               // in ns
//------------------------------------------------------------------------------
// default map, Richie says TS1 may have an old firmware with some bugs
//-----------------------------------------------------------------------------
  int adc_index_0[96] = {
    91, 85, 79, 73, 67, 61, 55, 49,
    43, 37, 31, 25, 19, 13,  7,  1,
    90, 84, 78, 72, 66, 60, 54, 48,
      
    42, 36, 30, 24, 18, 12,  6,  0,
    93, 87, 81, 75, 69, 63, 57, 51,
    45, 39, 33, 27, 21, 15,  9,  3,
      
    44, 38, 32, 26, 20, 14,  8,  2, 
    92, 86, 80, 74, 68, 62, 56, 50,
    47, 41, 35, 29, 23, 17, 11,  5,
      
    95, 89, 83, 77, 71, 65, 59, 53,
    46, 40, 34, 28, 22, 16, 10,  4,
    94, 88, 82, 76, 70, 64, 58, 52
  };
//-----------------------------------------------------------------------------
// TS1: compared to _0, swap lanes 3 and 4, and in the new lane3 swap lines 1 and 3
//-----------------------------------------------------------------------------
  int adc_index_1[96] = {
    91, 85, 79, 73, 67, 61, 55, 49,
    43, 37, 31, 25, 19, 13,  7,  1,
    90, 84, 78, 72, 66, 60, 54, 48,
      
    42, 36, 30, 24, 18, 12,  6,  0,
    93, 87, 81, 75, 69, 63, 57, 51,
    45, 39, 33, 27, 21, 15,  9,  3,
      
    94, 88, 82, 76, 70, 64, 58, 52,
    46, 40, 34, 28, 22, 16, 10,  4,
    95, 89, 83, 77, 71, 65, 59, 53,
      
    44, 38, 32, 26, 20, 14,  8,  2, 
    92, 86, 80, 74, 68, 62, 56, 50,
    47, 41, 35, 29, 23, 17, 11,  5
  };


  for (int i=0; i<96; i++) {
    _adc_index_0[adc_index_0[i]] = i;
    _adc_index_1[adc_index_1[i]] = i;
  }

//-----------------------------------------------------------------------------
// initialize reference channels, at this point use channels 91 and 94 for all 
// ROC's (the readout order is defined in firmware and is the same for all channels)
//-----------------------------------------------------------------------------
  _nActiveLinks        = _activeLinks.size();

  for (int roc=0; roc<kMaxNLinks; roc++) {
    _referenceChannel[roc][0] = 91;
    _referenceChannel[roc][1] = 94;
    if (roc < _nActiveLinks) {
      _referenceChannel[roc][0] = _refChCal[roc];
      _referenceChannel[roc][1] = _refChHV [roc];
    }

    _event_data.rdata[_station][_plane][roc].ref_ch[0] = &_event_data.rdata[_station][_plane][roc].channel[_referenceChannel[roc][0]];
    _event_data.rdata[_station][_plane][roc].ref_ch[1] = &_event_data.rdata[_station][_plane][roc].channel[_referenceChannel[roc][1]];
  }

  _tdc_bin             = (5/256.*1e-3);       // TDC bin width (Richie), in us
  _tdc_bin_ns          = _tdc_bin*1e3;        // convert to ns

  _initialized         = 0;

}

//-----------------------------------------------------------------------------
// I : channel number
//-----------------------------------------------------------------------------
void TrackerDQM::book_channel_histograms(art::TFileDirectory* Dir, int RunNumber, ChannelHist_t* Hist, int I) {

  Hist->nhits   = Dir->make<TH1F>(Form("ch_%02i_nhits"  ,I),Form("run %06i: ch %02i nhits"  ,RunNumber,I), 100, 0., 100.);
    
  Hist->time[0] = Dir->make<TH1F>(Form("ch_%02i_time0"  ,I),Form("run %06i: ch %02i time[0]",RunNumber,I),1000, 0., 100.);  // us
  Hist->time[1] = Dir->make<TH1F>(Form("ch_%02i_time1"  ,I),Form("run %06i: ch %02i time[0]",RunNumber,I),1000, 0., 100.);  // us

  Hist->t0  [0] = Dir->make<TH1F>(Form("ch_%02i_t0_0"   ,I),Form("run %06i: ch %02i t0[0]  ",RunNumber,I),1000,-20., 80.);  // ns
  Hist->t0  [1] = Dir->make<TH1F>(Form("ch_%02i_t0_1"   ,I),Form("run %06i: ch %02i t0[1]  ",RunNumber,I),1000,-20., 80.);  // ns

  Hist->t1  [0] = Dir->make<TH1F>(Form("ch_%02i_t1_0"   ,I),Form("run %06i: ch %02i t1[0]  ",RunNumber,I),1000,-20., 80.);  // ns
  Hist->t1  [1] = Dir->make<TH1F>(Form("ch_%02i_t1_1"   ,I),Form("run %06i: ch %02i t1[1]  ",RunNumber,I),1000,-20., 80.);  // ns
    
  Hist->tot [0] = Dir->make<TH1F>(Form("ch_%02i_tot0"   ,I),Form("run %06i: ch %02i tot[0]" ,RunNumber,I), 100, 0., 100.);
  Hist->tot [1] = Dir->make<TH1F>(Form("ch_%02i_tot1"   ,I),Form("run %06i: ch %02i tot[1]" ,RunNumber,I), 100, 0., 100.);
  Hist->pmp     = Dir->make<TH1F>(Form("ch_%02i_pmp"    ,I),Form("run %06i: ch %02i pmp"    ,RunNumber,I), 100, 0.,  10.);
  Hist->dt0     = Dir->make<TH1F>(Form("ch_%02i_dt0"    ,I),Form("run %06i: ch %02i T0(i+1)-T0(i)",RunNumber,I)      ,50000,  0.,50);
  Hist->dt1     = Dir->make<TH1F>(Form("ch_%02i_dt1"    ,I),Form("run %06i: ch %02i T1(i+1)-T1(i)",RunNumber,I)      ,50000,  0.,50);
  Hist->dt2     = Dir->make<TH1F>(Form("ch_%02i_dt2"    ,I),Form("run %06i: ch %02i T2(i+1)-T2(i)",RunNumber,I)      ,50000,  0.,50);
  Hist->dt0r    = Dir->make<TH1F>(Form("ch_%02i_dt0r_0" ,I),Form("run %06i: ch %02i T0(ich,0)-T0(ref,0)[0]",RunNumber,I),20000,-10.,10);
  Hist->dt1r    = Dir->make<TH1F>(Form("ch_%02i_dt1r_0" ,I),Form("run %06i: ch %02i T1(ich,0)-T1(ref,0)[0]",RunNumber,I),20000,-10.,10);
    
  for (int j=0; j<kMaxNHitsPerChannel; j++) {
    Hist->wf[j] = Dir->make<TH1F>(Form("h_wf_ch_%02i_%i",I,j),Form("run %06i: ch [%02i][%i] waveform",RunNumber,I,j),20, 0.,20.);
  }
}


//-----------------------------------------------------------------------------
void TrackerDQM::book_roc_histograms(art::TFileDirectory* Dir, int RunNumber, RocHist_t* Hist, int Link) {
  Hist->nbytes          = Dir->make<TH1F>("nbytes"  , Form("run %06i: n bytes"     ,RunNumber),10000,    0., 10000.);
  Hist->npackets        = Dir->make<TH1F>("npackets", Form("run %06i: n packets"   ,RunNumber), 1000,    0.,  1000.);
  Hist->nhits           = Dir->make<TH1F>("nhits"   , Form("run %06i: n hits"      ,RunNumber),  300,    0.,   300.);
  Hist->valid           = Dir->make<TH1F>("valid"   , Form("run %06i: valid"       ,RunNumber),    2,    0.,     2.);

  Hist->nh_vs_ch        = Dir->make<TH2F>("nh_vs_ch"  , Form("run %06i: nh vs ch"  ,RunNumber),  100,0.,100., 10,0,10);
  Hist->nh_vs_adc1      = Dir->make<TH2F>("nh_vs_adc1", Form("run %06i: nh vs adc" ,RunNumber),  100,0.,100., 10,0,10);

  Hist->dt0r_vs_ch      = Dir->make<TH2F>("dt0r_vs_ch_0", Form("run %06i: dt0r vs ch[0]",RunNumber),  100,0.,100.,2500,-25,25);
  Hist->dt1r_vs_ch      = Dir->make<TH2F>("dt1r_vs_ch_0", Form("run %06i: dt1r vs ch[0]",RunNumber),  100,0.,100.,2500,-25,25);

  Hist->dt0r01          = Dir->make<TH1F>("dt0r01", Form("run %06i: dt0r01"             ,RunNumber), 40000,-20000,20000);
  Hist->dt1r01          = Dir->make<TH1F>("dt1r01", Form("run %06i: dt1r01"             ,RunNumber), 40000,-20000,20000);

  Hist->nhits_vs_ich    = Dir->make<TH1F>("nh_vs_ich"  , Form("run %06i: nh vs ich"    ,RunNumber),  100, 0.,   100.);
  Hist->nhits_vs_adc[0] = Dir->make<TH1F>("nh_vs_adc_0", Form("run %06i: nh vs adc_0",RunNumber),  100, 0.,   100.);
  Hist->nhits_vs_adc[1] = Dir->make<TH1F>("nh_vs_adc_1", Form("run %06i: nh vs adc_1",RunNumber),  100, 0.,   100.);

  Hist->dt0rc_vs_ch[0]  = Dir->make<TH2F>("dt0rc_vs_ch_0", Form("run %06i: dt0rc vs ch[0], ns",RunNumber),  100,0.,100.,1000,-10,10);
  Hist->dt0rc_vs_ch[1]  = Dir->make<TH2F>("dt0rc_vs_ch_1", Form("run %06i: dt0rc vs ch[1], ns",RunNumber),  100,0.,100.,1000,-10,10);

  Hist->dt1rc_vs_ch[0]  = Dir->make<TH2F>("dt1rc_vs_ch_0", Form("run %06i: dt1rc vs ch[0], ns",RunNumber),  100,0.,100.,1000,-10,10);
  Hist->dt1rc_vs_ch[1]  = Dir->make<TH2F>("dt1rc_vs_ch_1", Form("run %06i: dt1rc vs ch[1], ns",RunNumber),  100,0.,100.,1000,-10,10);

  Hist->dt0rc_vs_adc[0] = Dir->make<TH2F>("dt0rc_vs_adc_0", Form("run %06i: dt0rc vs adc[0], ns",RunNumber),  100,0.,100.,1000,-10,10);
  Hist->dt0rc_vs_adc[1] = Dir->make<TH2F>("dt0rc_vs_adc_1", Form("run %06i: dt0rc vs adc[1], ns",RunNumber),  100,0.,100.,1000,-10,10);

  Hist->dt1rc_vs_adc[0] = Dir->make<TH2F>("dt1rc_vs_adc_0", Form("run %06i: dt1rc vs adc[0], ns",RunNumber),  100,0.,100.,1000,-10,10);
  Hist->dt1rc_vs_adc[1] = Dir->make<TH2F>("dt1rc_vs_adc_1", Form("run %06i: dt1rc vs adc[1], ns",RunNumber),  100,0.,100.,1000,-10,10);

  for (int i=0; i<kNChannels; i++) {
    art::TFileDirectory chan_dir = Dir->mkdir(Form("ch_%02i",i));
    book_channel_histograms(&chan_dir,RunNumber,&Hist->channel[i],i);
  }
}


//-----------------------------------------------------------------------------
void TrackerDQM::book_event_histograms(art::TFileDirectory* Dir, int RunNumber, EventHist_t* Hist) {
  Hist->nhits           = Dir->make<TH1F>("nhits"      , Form("run %06i: nhits total"  ,RunNumber), 1000, 0.,   1000.);
  Hist->nbtot           = Dir->make<TH1F>("nbtot"      , Form("run %06i: nbytes total" ,RunNumber), 1000, 0., 100000.);
  Hist->nfrag           = Dir->make<TH1F>("nfrag"      , Form("run %06i: n fragments"  ,RunNumber),  100, 0.,    100.);
  Hist->fsize           = Dir->make<TH1F>("fsize"      , Form("run %06i: fragment size",RunNumber), 1000, 0., 100000.);
  Hist->error           = Dir->make<TH1F>("error"      , Form("run %06i: error code"   ,RunNumber),   10, 0.,     10.);
  Hist->valid           = Dir->make<TH1F>("valid"      , Form("run %06i: valid code"   ,RunNumber),  100, 0.,    100.);
}

//-----------------------------------------------------------------------------
void TrackerDQM::book_histograms(int RunNumber) {
  art::ServiceHandle<art::TFileService> tfs;
    
  TLOG(TLVL_INFO) << "starting";

  art::TFileDirectory top_dir = tfs->mkdir("trk");

  book_event_histograms(&top_dir,RunNumber,&_hist.event);

  int station = 0;
  int plane   = 0;

  for (int i=0; i<_nActiveLinks; i++) {
    int link  = _activeLinks[i];
    art::TFileDirectory roc_dir = top_dir.mkdir(Form("roc_%i",link));
    book_roc_histograms(&roc_dir,RunNumber,&_hist.roc[station][plane][link],link);
  }
  
  TLOG(TLVL_INFO) << Form("pointer to the module: 0x%8p\n",(void*) this);
}

//-----------------------------------------------------------------------------
void TrackerDQM::beginJob() {
  TLOG(TLVL_INFO) << "starting";

  int           tmp_argc(2);
  char**        tmp_argv;

  tmp_argv    = new char*[2];
  tmp_argv[0] = new char[100];
  tmp_argv[1] = new char[100];

  strcpy(tmp_argv[0],"-b");
  strcpy(tmp_argv[1],Form("--web=server:%d",_port));

  _app = new TApplication("TrackerDQM", &tmp_argc, tmp_argv);

  // app->Run()
  // app_->Run(true);
  delete [] tmp_argv;

  _canvas[0] = new TCanvas("canvas_000");
  _canvas[1] = new TCanvas("canvas_001");

  _browser   = new TBrowser();

}

//-----------------------------------------------------------------------------
void TrackerDQM::endJob() {
  delete _canvas[0];
  delete _canvas[1];
}

//-----------------------------------------------------------------------------
void TrackerDQM::beginRun(const art::Run& aRun) {
  int rn  = aRun.run();

  if (_initialized != 0) return;
  _initialized = 1;

  book_histograms(rn);

  _canvas[0]->cd();
  _hist.event.nbtot->Draw();

  _canvas[1]->cd();
  _hist.event.nfrag->Draw();
}

//-----------------------------------------------------------------------------
// in the end of the run need to save histograms - will figure that out
//-----------------------------------------------------------------------------
void TrackerDQM::endRun(const art::Run& aRun) {
  //  int rn  = aRun.run();

  _initialized = 0;
}

// //-----------------------------------------------------------------------------
//   void TrackerDQM::fill_channel_histograms(ChannelHist_t* Hist, ChannelData_t* Data) {
//     Hist->nhits->Fill(Data->nhits);
//   }

//-----------------------------------------------------------------------------
  void TrackerDQM::fill_roc_histograms(RocHist_t* Hist, RocData_t* Rd) {
    
    Hist->nbytes->Fill  (Rd->nbytes);
    Hist->npackets->Fill(Rd->npackets);
    Hist->nhits->Fill   (Rd->nhits);
    Hist->valid->Fill   (Rd->valid);

    Hist->dt0r01->Fill  (Rd->dt0r01);
    Hist->dt1r01->Fill  (Rd->dt1r01);

//-----------------------------------------------------------------------------
// fill channel histograms
//-----------------------------------------------------------------------------
    for (int ich=0; ich<kNChannels; ich++) {
      ChannelData_t* chd = &Rd->channel[ich];
      ChannelHist_t* hch = &Hist->channel[ich];

      int fpga = _adc_index_1[ich] / 48;

      hch->nhits->Fill(chd->nhits);
      hch->dt0r->Fill(chd->dt0r);
      hch->dt1r->Fill(chd->dt1r);
            
      Hist->dt0r_vs_ch->Fill(ich,chd->dt0r);
      Hist->dt1r_vs_ch->Fill(ich,chd->dt1r);

      Hist->dt0rc_vs_ch[fpga]->Fill(ich,chd->dt0r_c);
      Hist->dt1rc_vs_ch[fpga]->Fill(ich,chd->dt1r_c);
            
      int iadc = _adc_index_1[ich];
      Hist->dt0rc_vs_adc[fpga]->Fill(iadc,chd->dt0r_c);
      Hist->dt1rc_vs_adc[fpga]->Fill(iadc,chd->dt1r_c);
//-----------------------------------------------------------------------------
// there are hits in this channel
//-----------------------------------------------------------------------------
      for (int ih=0; ih<chd->nhits; ih++) {
        mu2e::TrackerDataDecoder::TrackerDataPacket* hit = chd->hit[ih];

        uint32_t corr_tdc0 = correctedTDC(hit->TDC0());
        uint32_t corr_tdc1 = correctedTDC(hit->TDC1());

        hch->time[0]->Fill(corr_tdc0*_tdc_bin); // in us
        hch->time[1]->Fill(corr_tdc1*_tdc_bin); // in us

        hch->t0  [0]->Fill(corr_tdc0*_tdc_bin_ns);
        hch->t0  [1]->Fill(corr_tdc1*_tdc_bin_ns);

        hch->t1  [0]->Fill(_timeWindow-corr_tdc0*_tdc_bin_ns);
        hch->t1  [1]->Fill(_timeWindow-corr_tdc1*_tdc_bin_ns);

        hch->tot [0]->Fill(hit->TOT0);
        hch->tot [1]->Fill(hit->TOT1);
        hch->pmp    ->Fill(hit->PMP);
//-----------------------------------------------------------------------------
// waveforms in a given channel
//-----------------------------------------------------------------------------
        uint16_t adc[15];

        adc[ 0] = reverseBits(hit->ADC00);
        adc[ 1] = reverseBits(hit->ADC01A + (hit->ADC01B << 6));
        adc[ 2] = reverseBits(hit->ADC02);

        mu2e::TrackerDataDecoder::TrackerADCPacket* ahit = (mu2e::TrackerDataDecoder::TrackerADCPacket*) ((uint16_t*)hit+6);

        adc[ 3] = reverseBits(ahit->ADC0);
        adc[ 4] = reverseBits(ahit->ADC1A + (ahit->ADC1B << 6));
        adc[ 5] = reverseBits(ahit->ADC2);
        adc[ 6] = reverseBits(ahit->ADC3);
        adc[ 7] = reverseBits(ahit->ADC4A + (ahit->ADC4B << 6));
        adc[ 8] = reverseBits(ahit->ADC5);
        adc[ 9] = reverseBits(ahit->ADC6);
        adc[10] = reverseBits(ahit->ADC7A + (ahit->ADC7B << 6));
        adc[11] = reverseBits(ahit->ADC5);
        adc[12] = reverseBits(ahit->ADC6);
        adc[13] = reverseBits(ahit->ADC10A + (ahit->ADC10B << 6));
        adc[14] = reverseBits(ahit->ADC11);
        
        Hist->channel[ich].wf[ih]->Reset();
        for (int is=0; is<15; is++) {
          Hist->channel[ich].wf[ih]->Fill(is,adc[is]);
        }
      }
//-----------------------------------------------------------------------------
// time distance between the two sequential hits - need at least two
//-----------------------------------------------------------------------------
      for (int ih=1; ih<chd->nhits; ih++) {
        int corr_tdc0_ih  = (int) correctedTDC(chd->hit[ih  ]->TDC0());
        int corr_tdc1_ih  = (int) correctedTDC(chd->hit[ih  ]->TDC1());
        int corr_tdc0_ih1 = (int) correctedTDC(chd->hit[ih-1]->TDC0());
        int corr_tdc1_ih1 = (int) correctedTDC(chd->hit[ih-1]->TDC1());

        double dt0        = (corr_tdc0_ih-corr_tdc0_ih1)*_tdc_bin;
        double dt1        = (corr_tdc1_ih-corr_tdc1_ih1)*_tdc_bin;
        double dt2        = (dt0+dt1)/2;

        Hist->channel[ich].dt0->Fill(dt0);
        Hist->channel[ich].dt1->Fill(dt1);
        Hist->channel[ich].dt2->Fill(dt2);
      }
    }
//-----------------------------------------------------------------------------
// 
    for (int ich=0; ich<kNChannels; ich++) {
      int ind_0 = _adc_index_0[ich];
      int ind_1 = _adc_index_1[ich];

      int nh    = Rd->channel[ich].nhits;
                                        // number of hits in a channel vs the channel number
      Hist->nh_vs_ch->Fill(ich,nh);
      Hist->nh_vs_adc1->Fill(ind_1,nh);

      for (int ihit=0; ihit<nh; ihit++) {
        Hist->nhits_vs_ich->Fill(ich);
        Hist->nhits_vs_adc[0]->Fill(ind_0);
        Hist->nhits_vs_adc[1]->Fill(ind_1);
      }
    }

  }

//-----------------------------------------------------------------------------
void TrackerDQM::fill_event_histograms(EventHist_t* Hist, EventData_t* Data) {

  Hist->nbtot->Fill(Data->nbtot);
  Hist->nhits->Fill(Data->nhtot);
  Hist->nfrag->Fill(Data->nfrag);

  for (int i=0; i<Data->nfrag; i++) {
    int fsize = Data->fragments[i].nbytes;
    Hist->fsize->Fill(fsize);
  }
}

//-----------------------------------------------------------------------------
// fill_roc_histograms also fills the channel histograms
// if in error, only histogram the error code
//-----------------------------------------------------------------------------
int TrackerDQM::fill_histograms() {

  _hist.event.error->Fill(_event_data.error);
  _hist.event.valid->Fill(_event_data.valid);

  if (_event_data.error != 0) return -1;

  fill_event_histograms(&_hist.event,&_event_data);

  for (int ir=0; ir<_nActiveLinks; ir++) {
    int link = _activeLinks[ir];
    fill_roc_histograms(&_hist.roc[_station][_plane][link],&_event_data.rdata[_station][_plane][link]);
  }
  return 0;
}

//-----------------------------------------------------------------------------
// a fragment may have multiple ROC blocks
//-----------------------------------------------------------------------------
void TrackerDQM::analyze_fragment(const art::Event& Evt, const artdaq::Fragment* Fragment) {

  short* fdata = (short*) Fragment->dataBegin();

  _event_data.fragments.push_back(FragmentData_t());
  FragmentData_t* fdt = &_event_data.fragments.back();
//-----------------------------------------------------------------------------
// fragment size is specified in longs and includes service data, don't use
//-----------------------------------------------------------------------------
  fdt->nbytes  = fdata[0];
  TLOG(TLVL_DEBUG) << "fragment size: " << fdt->nbytes;

  if (fdata[0] > _maxFragmentSize) {
    _event_data.error = 3;
    TLOG(TLVL_ERROR) << Form("run:subrun:event %6i:%8i:%8i : ERROR:%i in %s: fdt->nbytes= %i, BAIL OUT\n",
                             Evt.run(),Evt.subRun(),Evt.event(),_event_data.error,__func__,fdt->nbytes);
    return;
  }
//-----------------------------------------------------------------------------
// start handling the ROC data
//-----------------------------------------------------------------------------
  short* first_address = fdata+_dataHeaderOffset; // offset is specified in 2-byte words
  short* last_address  = fdata+fdt->nbytes/2; // 

  TLOG(TLVL_DEBUG) << "first_address, last_address: " << first_address << " " << last_address;

  while (first_address < last_address) {

    DtcDataHeaderPacket_t* dh = (DtcDataHeaderPacket_t*) first_address;
    int link      = dh->ROCID;

    TLOG(TLVL_DEBUG) << "(" << __LINE__ << ") link:" << link ;
//-----------------------------------------------------------------------------
// check link number
//-----------------------------------------------------------------------------
    int found = 0;
    for (int i=0; i<_nActiveLinks; i++) {
      if (_activeLinks[i] == link) {
        found = 1;
        break;
      }
    }

    if (found == 0) {
      _event_data.error = 4;
      TLOG(TLVL_ERROR) << Form("event %6i:%8i:%8i : ERROR=%i link=%i, BAIL OUT\n",
                               Evt.run(),Evt.subRun(),Evt.event(),_event_data.error,link);
      return;
    }

    RocData_t* rd = &_event_data.rdata[_station][_plane][link];
//-----------------------------------------------------------------------------
// for a given FPGA, a reference channel is the first channel in the readout order
//-----------------------------------------------------------------------------
    ChannelData_t* ref_ch[2];

    ref_ch[0]     = &rd->channel[_referenceChannel[link][0]];
    ref_ch[1]     = &rd->channel[_referenceChannel[link][1]];
        
    rd->nbytes    = dh->byteCount;
    rd->npackets  = dh->nPackets;
    rd->nhits     = dh->nPackets/2;         //  printf("nhits : %3i\n",nhits);
    rd->valid     = dh->valid;
    rd->dt0r01    = -1.e12;
    rd->dt1r01    = -1.e12;
    
    _event_data.nhtot += rd->nhits;
    _event_data.valid += dh->valid*10;
      
    for (int i=0; i<kNChannels; i++) {
      rd->channel[i].nhits = 0;
    }

    for (int ihit=0; ihit<rd->nhits; ihit++) {
//-----------------------------------------------------------------------------
// first packet, 16 bytes, or 8 ushort's is the data header packet
//-----------------------------------------------------------------------------
      mu2e::TrackerDataDecoder::TrackerDataPacket* hit ;
      hit     = (mu2e::TrackerDataDecoder::TrackerDataPacket*) (fdata+ihit*0x10+_dataHeaderOffset+0x08);
      int ich = hit->StrawIndex;
      
      if (ich > 128) ich = ich-128;
      
      if (ich > 95) {
        _event_data.error = 1;
        TLOG(TLVL_ERROR) << Form("event %6i:%8i:%8i : ERROR:%i in %s: link = %i ich = %i, BAIL OUT\n",
                                 Evt.run(),Evt.subRun(),Evt.event(),_event_data.error,__func__,link,ich);
        return;
      }
      
      ChannelData_t* chd = &rd->channel[ich];

      int nh = chd->nhits;
      if (nh >= kMaxNHitsPerChannel) {
        _event_data.error = 2;
        TLOG(TLVL_ERROR) << Form("event %6i:%8i:%8i : ERROR:%i in %s: link = %i ich = %i, N(hits) >= %i BAIL OUT\n",
                                 Evt.run(),Evt.subRun(),Evt.event(),_event_data.error,__func__,link,ich,kMaxNHitsPerChannel);
        return;
      }
      
      chd->hit[nh]   = hit;
      chd->nhits    += 1;
    }
//-----------------------------------------------------------------------------
// hits in all channels counted
// time difference between a channel and a reference channel
//-----------------------------------------------------------------------------
    for (int i=0; i<kNChannels; i++) {
      ChannelData_t* chd = &rd->channel[i];

      int nh   = chd->nhits;
      int fpga = _adc_index_1[i] / 48;

      ChannelData_t* rch = ref_ch[fpga];
//-----------------------------------------------------------------------------
// in most cases, the number of hits in the reference channel should be greater 
// than the number of channels in any other channel of a given FPGA
//-----------------------------------------------------------------------------
      int iref = _referenceChannel[link][fpga];
      int nhr = rd->channel[iref].nhits;
      if ((nhr > 0) and (nh > 0)) {
//-----------------------------------------------------------------------------
// at least one hit in both reference and test channels
//-----------------------------------------------------------------------------
        int t0r = correctedTDC(rch->hit[0]->TDC0());
        int t1r = correctedTDC(rch->hit[0]->TDC1());
        int t0  = correctedTDC(chd->hit[0]->TDC0());
        int t1  = correctedTDC(chd->hit[0]->TDC1());
          
        float dt_over_2(_dt/2);
          
        chd->dt0r   = (t0-t0r)*_tdc_bin_ns;        // convert to ns  

        chd->dt0r_c = chd->dt0r;
        if (chd->dt0r >  dt_over_2/2) chd->dt0r_c = chd->dt0r + _gen_offset[i] - _dt;
        if (chd->dt0r < -dt_over_2/2) chd->dt0r_c = chd->dt0r + _gen_offset[i];
        
        chd->dt1r   = (t1-t1r)*_tdc_bin_ns;        // convert to ns

        chd->dt1r_c = chd->dt1r;
        if (chd->dt1r >  dt_over_2/2) chd->dt1r_c = chd->dt1r + _gen_offset[i] - _dt;
        if (chd->dt1r < -dt_over_2/2) chd->dt1r_c = chd->dt1r + _gen_offset[i];
      }
    }
//-----------------------------------------------------------------------------
// time offset between the two pulsers for the same ROC
//-----------------------------------------------------------------------------
    if ((rd->ref_ch[0]->nhits > 0) and (rd->ref_ch[1]->nhits > 0)) {
      int t0r0   = correctedTDC(rd->ref_ch[0]->hit[0]->TDC0());
      int t1r0   = correctedTDC(rd->ref_ch[0]->hit[0]->TDC1());
      
      int t0r1   = correctedTDC(rd->ref_ch[1]->hit[0]->TDC0());
      int t1r1   = correctedTDC(rd->ref_ch[1]->hit[0]->TDC1());
      
      rd->dt0r01 = (t0r0-t0r1)*_tdc_bin_ns;        // convert to ns  
      rd->dt1r01 = (t1r0-t1r1)*_tdc_bin_ns;        // convert to ns  
    }
//-----------------------------------------------------------------------------
// address in 2-byte words (N(data packets)+data header packet)
//-----------------------------------------------------------------------------
    first_address += (dh->nPackets + 1)*8;
  }
}

//--------------------------------------------------------------------------------
// assume that we only have tracker fragment(s)
//-----------------------------------------------------------------------------
void TrackerDQM::analyze(const art::Event& event) {

  _event_data.nbtot = 0;
  _event_data.nhtot = 0;
  _event_data.nfrag = 0;
  _event_data.error = 0;
  _event_data.valid = 0;

  _event_data.fragments.clear();

  // for (int i=0; i<100; i++) _nwf[i] = 0;

//-----------------------------------------------------------------------------
// first get all fragments, select Tracker ones
//-----------------------------------------------------------------------------
  auto handle = event.getValidHandle<std::vector<artdaq::Fragment> >("daq:TRK");
//-----------------------------------------------------------------------------
// calculate the fragment size manually - big thank you to designers (:
//----------------------------------------------------------------------------- 
  int ifrag = 0;

  for (const artdaq::Fragment& frag : *handle) {
    ushort* buf    = (ushort*) (frag.dataBegin());
    int     nbytes = buf[0];
    int     fsize  = frag.sizeBytes();

    if (nbytes < 2) {
      _event_data.error = 5;
      TLOG(TLVL_ERROR) << Form("event %6i:%8i:%8i : ERROR:%i in %s: nbytes >= %i BAIL OUT\n",
                               event.run(),event.subRun(),event.event(),_event_data.error,
                               __func__,nbytes);
    }

    _event_data.nfrag += 1;
    _event_data.nbtot += nbytes;        // including artdaq part

    if ((_event_data.error == 0) and _analyzeFragments) analyze_fragment(event,&frag);

    if (_diagLevel > 2) {
      printf("%s: ---------- TRK fragment # %3i nbytes: %5i fsize: %5i\n",__func__,ifrag,nbytes,fsize);
      printFragment(&frag,nbytes/2);
    }
    ifrag++;
  }
//-----------------------------------------------------------------------------
// proxy for event histograms
//-----------------------------------------------------------------------------
  if (_diagLevel > 1) {
    if ((_event_data.nbtot >= _minNBytes) and (_event_data.nbtot <= _maxNBytes)) {
      TLOG(TLVL_INFO) << Form(" Run : %5i subrun: %5i event: %8i nfrag: %3i nbytes: %5i\n", 
                              event.run(),event.subRun(),event.event(), _event_data.nfrag, _event_data.nbtot);
    }
  }
//-----------------------------------------------------------------------------
// go into interactive mode, 
// fInteractiveMode = 0 : do not stop
// fInteractiveMode = 1 : stop after each event (event display mode)
// fInteractiveMode = 2 : stop only in the end of run, till '.q' is pressed
//-----------------------------------------------------------------------------
// TModule::analyze(event);
//-----------------------------------------------------------------------------
// event data un(re)packed , fill histograms
//-----------------------------------------------------------------------------
  if (_analyzeFragments != 0) {
    fill_histograms();
  }
//-----------------------------------------------------------------------------
// DTC registers
//-----------------------------------------------------------------------------
  if (_dumpDTCRegisters) {
    auto h = event.getValidHandle<std::vector<artdaq::Fragment>>("daq:TRKDTC");

    for (const artdaq::Fragment& frag : *h) {
      int *buf  = (int*) (frag.dataBegin());
      int nreg  = buf[0];
      int fsize = frag.sizeBytes();
      printf("%s: -------- DTC registers dump n(reg)=%5i size: %5i\n",__func__,nreg,fsize);
      printFragment(&frag,2+4*nreg);
    }
  }

  gSystem->ProcessEvents();
}


DEFINE_ART_MODULE(TrackerDQM)
