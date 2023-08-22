
# cet_build_plugin(Occupancy art::module LIBRARIES REG
#   )

# cet_build_plugin(TrackerDQM art::module LIBRARIES REG
#   )

cet_build_plugin(CaloDQM art::module LIBRARIES REG
art_root_io::TFileService_service
artdaq_core_mu2e::artdaq-core-mu2e_Data
otsdaq_mu2e::otsdaq-mu2e_ArtModules
otsdaq::NetworkUtilities
ROOT::Hist
ROOT::Tree
ROOT::Core
ROOT::RIO
ROOT::Gui
)

cet_build_plugin(IntensityInfoDQM art::module LIBRARIES REG
art_root_io::TFileService_service
artdaq_core_mu2e::artdaq-core-mu2e_Data
otsdaq_mu2e::otsdaq-mu2e_ArtModules
otsdaq::NetworkUtilities
ROOT::Hist
ROOT::Tree
ROOT::Core
ROOT::RIO
ROOT::Gui
)

cet_build_plugin(TrackerDQM art::module LIBRARIES REG
art_root_io::TFileService_service
artdaq_core_mu2e::artdaq-core-mu2e_Data
otsdaq_mu2e::otsdaq-mu2e_ArtModules
otsdaq::NetworkUtilities
ROOT::Hist
ROOT::Tree
ROOT::Core
ROOT::RIO
ROOT::Gui
)

cet_build_plugin(TriggerDQM art::module LIBRARIES REG
art_root_io::TFileService_service
artdaq_core_mu2e::artdaq-core-mu2e_Data
canvas::canvas
otsdaq_mu2e::otsdaq-mu2e_ArtModules
otsdaq::NetworkUtilities
mu2e_Mu2eUtilities
ROOT::Hist
ROOT::Tree
ROOT::Core
ROOT::RIO
ROOT::Gui
)

cet_build_plugin(STMDQM art::module LIBRARIES REG
art_root_io::TFileService_service
artdaq_core_mu2e::artdaq-core-mu2e_Data
otsdaq_mu2e::otsdaq-mu2e_ArtModules
otsdaq::NetworkUtilities
ROOT::Hist
ROOT::Tree
ROOT::Core
ROOT::RIO
ROOT::Gui
)





install_headers()
install_source()
