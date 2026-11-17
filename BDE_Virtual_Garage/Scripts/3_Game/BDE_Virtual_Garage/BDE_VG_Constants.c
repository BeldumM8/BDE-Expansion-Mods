static const int BDE_VG_MENU_ID = 78142;
static const float BDE_VG_ACCESS_ACTION_DISTANCE = 4.0;

static const int BDE_VG_CALLBACK_UPDATE = 1;
static const int BDE_VG_CALLBACK_STORED = 2;
static const int BDE_VG_CALLBACK_RETRIEVED = 3;
static const int BDE_VG_CALLBACK_ERROR = 4;

static const int BDE_VG_KIND_LAND = 0;
static const int BDE_VG_KIND_BOAT = 1;
static const int BDE_VG_KIND_AIRCRAFT = 2;

static const string BDE_VG_PROFILE_ROOT = "$profile:BDE";
static const string BDE_VG_PROFILE_DIRECTORY = "$profile:BDE/BDE_Virtual_Garage";
// LOCAL PER-MAP storage. Do not include this folder in Private Hive sync.
static const string BDE_VG_VEHICLE_DATA_DIRECTORY = "$profile:BDE/BDE_Virtual_Garage/_VehicleData/";
// ONLY this folder is Hive-synced: opt-in trader vehicle owner JSONs, root
// entity .bin files, and nested cargo/attachment .bin folders.
static const string BDE_VG_CROSSMAP_DATA_DIRECTORY = "$profile:BDE/BDE_Virtual_Garage/_CrossMapVehicleData/";
static const string BDE_VG_SETTINGS_FILE = "$profile:BDE/BDE_Virtual_Garage/Settings.json";
// Map-local: keep OUT of the Private Hive sync rules.
static const string BDE_VG_SPAWN_POSITIONS_FILE = "$profile:BDE/BDE_Virtual_Garage/SpawnPositions.json";
static const string BDE_VG_LOG_FILE = "$profile:BDE/BDE_Virtual_Garage/VirtualGarage.log";

static const string BDE_VG_LEGACY_METADATA_DIRECTORY = "$profile:BDE/BDE_Virtual_Garage/Metadata/";
static const string BDE_VG_LEGACY_VEHICLE_DIRECTORY = "$profile:BDE/BDE_Virtual_Garage/Vehicles/";


