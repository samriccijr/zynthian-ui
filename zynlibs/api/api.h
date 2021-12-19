/*  Zynthian core API */

/*  Mixer
    There is a stereo summing mixer with one stereo channel strip per chain
    Each channel strip is identified by the associated chain index
    Each channel strip has control of level, balance, mute, solo- & mono (all inputs mixed to both outputs)
    There is a main mix bus with similar control to which all channels are routed
    Instantaneous peak programme and held peak programme for each leg (left/right) of each channel is available
    There is a callback mechanism for state change
*/

/** @brief  Get mixer main mix bus index
*   @retval int Index of channel strip associated with main mix bus
*   @note   This is synonymous with maximum quantity of chains and will change if setMaxChains() is called
*/
int getMainMixbus();

/** @brief  Get mixer channel fader level
*   @param  channel Index of channel
*   @retval float Fader value [0.0..1.0]
*/
float getFaderLevel(int channel);

/** @brief  Set mixer fader level
*   @param  channel Index of channel
*   @param  value Fader value
*/
void setFaderLevel(int channel, float value);

/** @brief  Get mixer channel balance
*   @param  channel Index of channel
*   @retval float Balance value [-1.0..1.0]
*   @note   Balance for stereo source, pan for mono source
*/
float getBalance(int channel);

/** @brief  Set mixer channel balance
*   @param  channel Index of channel
*   @param  value Balance value [-1.0..1.0]
*/
void setBalance(int channel, float value);

/** @brief  Get mixer channel mute state
*   @param  channel Index of channel
*   @retval bool True if channel muted
*/
bool getMute(int channel);

/** @brief  Set mixer channel mute state
*   @param  channel Index of channel
*   @param  state True to mute channel
*/
void setMute(int channel, bool state);

/** @brief  Get mixer channel solo state
*   @param  channel Index of channel
*   @retval bool True if channel soloed
*/
bool getSolo(int channel);

/** @brief  Set mixer channel solo state
*   @param  channel Index of channel
*   @param  state True to solo channel
*   @note   Solo state is  accumulative, i.e. may solo several channels
*   @note   Solo main mixbus will disable all channel solo*/
void setSolo(int channel, bool value);

/** @brief  Get mixer channel mono state
*   @param  channel Index of channel
*   @retval bool True if channel monoed
*/
bool getMono(int channel);

/** @brief  Set mixer channel mono state
*   @param  channel Index of channel
*   @param  state True to mono channel
*   @note   Mono channel will mix inputs to both (left & right) main mix bus legs
*/
void setMono(int channel, bool value);

/** @brief  Get mixer channel instantaneous audio level
*   @param  channel Index of channel
*   @param  leg 0 for left, 1 for right
*   @retval float Instantaneous audio level [0..-200dBFS]
*/
float getPeakLevel(int channel, int leg);

/** @brief  Get mixer channel peak hold audio level
*   @param  channel Index of channel
*   @param  leg 0 for left, 1 for right
*   @retval float Peak hold audio level [0..-200dBFS]
*/
float getPeakHold(int channel, int leg);

/** @brief  Register a callback for mixer state changes
*   @param  callback Pointer to callback function
*   @param  bitmask Bitmask flag indication of parameters to monitor [0:Fader, 1:Mute, 2:Solo, 4:Mono, 8:Peak Audio, 16:Peak Hold] (Default: All)
*/
void registerMixer(void* callback, uint32_t bitmask=0xFFFFFFFF);

/** @brief  Unregister a callback for mixer state changes
*   @param  callback Pointer to callback function
*   @param  bitmask Bitmask flag indication of parameters to unregister [0:Fader, 1:Mute, 2:Solo, 4:Mono, 8:Peak Audio, 16:Peak Hold] (Default: All) (Default: All)
*/
void unregisterMixer(void* callback, uint32_t bitmask=0xFFFFFFFF);

/*  Chains
    A chain is a set of engines with audio and control signal interlinks
    Each chain with audio processing has an associated mixer channel
    Chains are identified by an integer index
    Chain has a rectangular grid / graph of virtual slots into which engines may be placed
    Empty slots are assumed to connect adjacent horizontal slot signals
    A special virtual engine joins adjacent vertical slot signals
*/

/** @brief  Get the maximum quantity of chains
*   @retval uint16_t Maximum quantity of channel strips
*   @note   Attempt to access higher chain index will fail silently
*/
uint16_t getMaxChain();

/** @brief  Set maximum quantity of chains
*   @param  max Maximum quantity of chains
*   @note   Chains and associated mixer strips with higher indicies will be removed
*/
void setMaxChains(uint16_t max);

/** @brief  Get quantity of chains
*   @retval uint16_t Quantity of chains defined in current snapshot
*/
uint16_t getChainCount();

/** @brief  Get chain name
*   @param  chain Index of chain
*   @retval string Name of chain
*/
string getChainName(uint16_t chain);

/** @brief  Set chain name
*   @param  chain Index of chain
*   @param  string Name of chain
*/
void setChainName(uint16_t chain, string name);

/** @brief  Get bitmask of MIDI channels assinged to chain
*   @param  chain Index of chain
*   @retval uint16_t Bitmask of MIDI channels
*/
uint16_t getChainMidiChannel(uint16_t chain);

/** @brief  Set chain MIDI channel
*   @param  chain Index of chain
*   @param  channel MIDI channel [0..16, -1 to disable MIDI]
*/
void setChainMidiChannel(uint16_t chain, int channel);

/** @brief  Set chain MIDI channels
*   @param  chain Index of chain
*   @param  channels Bitmask of MIDI channels
*   @todo   Handle multiple pipes
*/
void setChainMidiChannels(uint16_t chain, uint16_t channels);

/** @brief  Get chain note range filter minimum note value
*   @param  chain Index of chain
*   @retval uint8_t MIDI note value of lowest note sent to chain
*/
uint8_t getChainNoteMin(uint16_t chain);

/** @brief  Get chain note range filter maximum note value
*   @param  chain Index of chain
*   @retval uint8_t MIDI note value of highest note sent to chain
*/
uint8_t getChainNoteMax(uint16_t chain);

/** @brief  Set chain note range filter minimum note value
*   @param  chain Index of chain
*   @param  min MIDI note value of lowest note passed to chain [0..127, -1 for no change]
*   @param  max MIDI note value of highest note passed to chain [0..127, -1 for no change]
*   @note   max should be greater or equal to min otherwise max is ignored
*/
void setChainNoteRange(uint16_t chain, int8_t min, int8_t max);

/** @brief  Get chain MIDI transpose
*   @param  chain Index of chain
*   @retval int8_t Transpose value in MIDI note steps [-127..127]
*/
int8_t getChainTranspose(uint16_t chain);

/** @brief  Set chain MIDI transpose
*   @param  chain Index of chain
*   @param  transpose MIDI notes to transpose [-127..127]
*/
void setChainTranspose(uint16_t chain, int8_t transpose);

/** @brief  Get quantity of engines in a chain
*   @param  chain Index of chain
*   @retval uint32_t Quantity of engines in chain
*/
uint32_t getEngineCount(uint16_t chain);

/** @brief  Get quantity of rows in chain graph
*   @param  chain Index of chain
*   @retval uint8_t Quantity of rows in graphdefaults
*/
uint8_t getChainRows(uint16_t chain);

/** @brief  Get quantity of columns in chain graph
*   @param  chain Index of chain
*   @retval uint8_t Quantity of columns in graphdefaults
*/
uint8_t getChainColumns(uint16_t chain);


/** @brief  Get id of engine within a chain
*   @param  chain Index of chain
*   @param  row Vertical position of engine within chain graph
*   @param  col Horizontal position of engine within chain graph
*   @retval uint32_t Id of engine
*   @note   Id is chain << 16 | col << 8 | row
*/
uint32_t getEngine(uint16_t chain, uint8_t row, uint8_t column);

/** @brief  Remove an engine from a chain
*   @param  engine Id of engine
*   @note   Engine instance is destroyed
*/
void removeEngine(uint32_t engine);

/** @brief  Add an engine to a chain
*   @param  chain Index of chain
*   @param  row Vertical position of engine within chain graph
*   @param  col Horizontal position of engine within chain graph
*   @param  class Name of engine class being added
*   @retval uint32_t Id of engine or 0xFFFFFFFF if engine cannot be instantiated
*   @note   Engine instance is instantiated with default parameters and connected to adjacent horizontal slots
*   @note   Replaces and destroys any existing engine at same location in graph
*   @note   Use special classes JOIN_INPUT, JOIN_OUTPUT, JOIN_BOTH to connect input / output of horizontally adjacent slots to vertically adjacent slots
*   @note   JOIN classes give hints to autorouter which may be overriden by direct audio/MIDI routing of individual signals
*/
uint32_t addEngine(uint16_t chain, uint8_t row, uint8_t column, string class);

/** @brief  Moves an engine to a new position in a chain
*   @param  engine Id of engine
*   @param  chain New chain id
*   @param  row New row position
*   @param  column New column
*/
void moveEngine(uint16_t chain, uint8_t row, uint8_t column);

/** @brief  Copies (clones) an engine to a new position in a chain
*   @param  engine Id of engine to copy
*   @param  chain New chain id
*   @param  row New row position
*   @param  column New column
*/
void copyEngine(uint16_t chain, uint8_t row, uint8_t column);


/*  Engines
    Engines are instances of Engine Classes
    Each chain consists of zero or more engines
*/

/** @brief  Get the class of engine within chain 
*   @param  engine Id of engine
*   @retval string Class name of engine
*/
string getEngineClass(uint32_t engine);

/** @brief  Get MIDI channel for control assigned to engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of paramter
*   @retval uint8_t MIDI channel
*   @todo   Do we allow multiple controls assigned to single parameter? If so we need to index them. Should we abstract control layer?
*/
uint16_t getEngineParameterMidiChannel(int engine, string parameter); 

/** @brief  Get MIDI control assigned to engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of paramter
*   @retval uint8_t MIDI CC
*   @todo   Do we allow multiple controls assigned to single parameter? If so we need to index them.
*/
uint16_t getEngineParameterMidiControl(int engine, string parameter); 

/** @brief  Assigns a MIDI CC to control an engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @param  uint8_t channel MIDI channel
*   @param  uint8_t cc MIDI continuous controller
*   @todo   May wish to add more complex filtering, scaling, offset, etc.
*/
void addEngineParameterMidiControl(uint32_t engine, string parameter, int channel, int cc);

/** @brief  Unassigns a MIDI CC from controlling an engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of parameter ("ALL" to remove all parameters)
*   @param  channel MIDI channel [0..15, 0xFF for all channels]
*   @param  cc MIDI continuous controller [0..127, 0xFF for all controllers]
*/
void removeEngineMidiControl(uint32_t engine, string parameter, int channel, int cc);

/** @brief  Get the index of an engine's currently loaded preset
*   @param  engine Id of engine
*   @retval int Preset index or -1 if no preset loaded
*   @todo   If we allow only one control assigned to each parameter we don't need to pass MIDI parameters here
*/
int getEnginePreset(uint32_t engine);

/** @brief  Get the index of the bank of an engine's currently loaded preset
*   @param  engine Id of engine
*   @retval int Bank index or -1 if no preset loaded or engine does not support banks
*/
int getEngineBank(uint32_t engine);

/** @brief  Request an engine loads / selects a preset
*   @param  engine Id of engine
*   @param  bank Index of bank (ignored if bank not supported or required)
*   @param  preset Index of preset
*/
void selectEnginePreset(uint32_t engine, int bank, int preset);

/** @brief  Add currently selected preset to engine class
*   @param  engine Id of engine containing current configuration
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset (replaces existing preset)
*   @param  name Name of new preset
*   @note   The parameters and configuration of selected engine are used
*/
void storeEnginePreset(uint32_t engine, uint32_t bank, uint32_t preset, string name);

/** @brief  Check if engine parameters differ from currently loaded preset
*   @brief  engine Id of engine
*   @retval bool True if preset modified
*/
bool isEngineModified(uint32_t engine);

/** @brief  Get an engine parameter value
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @retval float Parameter value (zero if conversion from naitive data type fails)
*/
float getEngineParameterAsFloat(uint32_t engine, string parameter);

/** @brief  Set an engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @param  value Value of parameter
*   @note   No change if conversion to naitive data type fails
*/
void setEngineParameterAsFloat(uint32_t engine, string parameter, float value);

/** @brief  Get an engine parameter value
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @retval int Parameter value (zero if conversion from naitive data type fails)
*/
int getEngineParameterAsInt(uint32_t engine, string parameter);

/** @brief  Set an engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @param  value Value of parameter
*   @note   No change if conversion to naitive data type fails
*/
void setEngineParameterAsInt(uint32_t engine, string parameter, int value);

/** @brief  Get an engine parameter value
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @retval string Parameter value (empty string if conversion from naitive data type fails)
*/
string getEngineParameterAsString(uint32_t engine, string parameter);

/** @brief  Set an engine parameter
*   @param  engine Id of engine
*   @param  parameter Name of parameter
*   @param  value Value of parameter
*   @note   No change if conversion to naitive data type fails
*/
void setEngineParameterAsString(uint32_t engine, string parameter, string value);

/** @brief  Get the chain an engine belongs to
*   @param  engine Id of engine
*   @retval uint16_t Id of chain or 0xFFFF for invalid engine id
*/
uint16_t getEngineChain(int engine);

/** @brief  Get chain row an engine is positioned
*   @param  engine Id of engine
*   @retval uint8_t Row or 0xFF for invalid engine id
*/
uint8_t getEngineRow(int engine);

/** @brief  Get chain column an engine is positioned
*   @param  engine Id of engine
*   @retval uint8_t Column or 0xFF for invalid engine id
*/
uint8_t getEngineColumn(int engine);

/*  Engine Classes
    Classes or types of different engines
    May be audio or MIDI (or other control signal) generators
    May be audio or MIDI (or other control signal) effects or processors
*/

/** @brief  Get quantity of engine classes
*   @retval uint32_t Quantity of supported engine classes
*/
uint32_t getEngineClassCount();

/** @brief  Get name of engine class
*   @param  index Index of engine class
*   @retval string Name of engine class
*   @note   Allows iteration to detect class names
*/
string getEngineClass(int index);

/** @brief  Get engine class type
*   @param  class Name of engine class
*   @retval string Engine class type [Audio effect | MIDI effect | Audio generator | etc. Should this be an enumeration integer?]
*/
string getEngineClassType(string class); 

/** @brief  Get quantity of signal inputs of an engine class
*   @param  class Name of engine class
*   @retval uint8_t Quantity of signal inputs
*/
uint8_t getEngineClassInputCount(string class);

/** @brief  Get quantity of signal outputs of an engine class
*   @param  class Name of engine class
*   @retval uint8_t Quantity of signal outputs
*/
uint8_t getEngineClassOutputCount(string class);

/** @brief  Get quantity of banks available to an engine class
*   @param  class Name of engine class
*   @retval uint32_t Quantity of banks
*/
uint32_t getEngineClassBankCount(string class);

/** @brief  Get name of an engine class bank
*   @param  class Name of engine class
*   @param  bank Index of bank
*   @retval string Name of bank
*/
string getEngineClassBankName(string class, uint32_t bank);

/** @brief  Set name of an engine class bank
*   @param  class Name of engine class
*   @param  bank Index of bank
*   @param  name New name for bank
*   @todo   Should it be permissible to rename banks? Could this break engines / snapshots?
*/
void setEngineClassBankName(string class, uint32_t bank, string name);

/** @brief  Add a bank to an engine class
*   @param  class Name of engine class
*   @param  name Name of new bank
*/
void addEngineClassBank(string class, string name);

/** @brief  Remove a bank from an engine class
*   @param  class Name of engine class
*   @param  name Name of bank to remove
*   @note   Presets within bank are destroyed
*/
void removeEngineClassBank(string class, string name);

/** @brief  Get quantity of presets within an engine class bank
*   @param  class Name of engine class
*   @param  bank Index of bank
*   @retval uint32_t Quantity of presets
*   @todo   Should we use bank name to identify bank?
*/
uint32_t getEngineClassPresetCount(string class, uint32_t bank);

/** @brief  Get name of an engine class preset
*   @param  class Name of engine class
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset
*   @retval string Name of class or empty string if preset does not exist
*   @todo   Should we use bank name to identify bank?
*/
string getEngineClassPresetName(string class, uint32_t bank, uint32_t preset);

/** @brief  Removes preset from engine class
*   @param  class Name of engine class
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset (replaces existing preset)
*/
void removeEngineClassPreset(string class, uint32_t bank, uint32_t preset);

/** @brief  Get quantity of favourite presets within an engine class bank
*   @param  class Name of engine class (may be empty to select all classes)
*   @retval uint32_t Quantity of presets
*/
uint32_t getFavouritePresetCount(string class, uint32_t bank);

/** @brief  Add preset to favourites
*   @param  class Name of engine class
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset
*/
void addFavouritePreset(string class, uint32_t bank, uint32_t preset);

/** @brief  Remove preset from favourites
*   @param  class Name of engine class
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset
*/
void removeFavouritePreset(string class, uint32_t bank, uint32_t preset);

/** @brief  Get favourte preset class
*   @param  favourite Index of favourite
*   @retval string Name of class or empty string if favourite does not exist
*/
string getFavouriteClass(uint32_t favourite);

/** @brief  Get favourte preset bank
*   @param  favourite Index of favourite
*   @retval uint32_t Index of bank within which favourite resides
*/
uint32_t getFavouriteBank(uint32_t favourite);

/** @brief  Get favourte preset preset
*   @param  favourite Index of favourite
*   @retval uint32_t Index of preset
*/
uint32_t getFavouritePreset(uint32_t favourite);

/** @brief  Check if a preset is a favourite
*   @param  class Name of engine class
*   @param  bank Index of bank (ignored if class does not support banks)
*   @param  preset Index of preset
*   @retval bool True if preset is a favourite
*/
bool isFavourite(string class, uint32_t bank, uint32_t preset);

/** @brief  Get quantity of parameters for an engine class
*   @param  class Name of engine class
*   @retval uint32_t Quantity of parameters the engine class exposes
*/
uint32_t getEngineClassParameterCount(string class);

/** @brief  Get an engine class parameter name
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval string Parameter name
*/
string getEngineClassParameterName(string class, uint32_t parameter);

/** @brief  Get an engine class parameter type
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval string Parameter type
*/
string getEngineClassParameterType(string class, uint32_t parameter);

/** @brief  Get an engine class parameter minimum value
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval float Minimum value (0 if not valid)
*/
float getEngineClassParameterMinimum(string class, uint32_t parameter);

/** @brief  Get an engine class parameter maximum value
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval float Maximum value (0 if not valid)
*/
float getEngineClassParameterMaximum(string class, uint32_t parameter);

/** @brief  Get size of step a class parameter value may change by
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval float Parameter step size (0.0 if not valid)
*/
float getEngineClassParameterStep(string class, uint32_t parameter);

/** @brief  Get class parameter units
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval string Parameter units (empty string if not valid)
*/
float getEngineClassParameterUnits(string class, uint32_t parameter);

/** @brief  Get class parameter group
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval string Name of group parameter belongs (empty string if not valid)
*/
float getEngineClassParameterGroup(string class, uint32_t parameter);

/** @brief  Get quantity of class parameter enumeration values
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @retval uint32_t Quantity of enumeration values (0 if not valid)
*/
uint32_t getEngineClassParameterEnums(string class, uint32_t parameter);

/** @brief  Get class parameter enumeration name
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @param  enum Index of the enumeration
*   @retval string Name of the indexed enumeration (empty string if not valid)
*/
string getEngineClassParameterEnums(string class, uint32_t parameter, uint32_t enum);

/** @brief  Get class parameter enumeration value
*   @param  class Name of engine class
*   @param  parameter Index of the parameter
*   @param  enum Index of the enumeration
*   @retval string value of the indexed enumeration (empty string if not valid)
*   @todo   Should we add methods for each type of enum: string, float, integer?
*/
string getEngineClassParameterEnums(string class, uint32_t parameter, uint32_t enum);


/*  Routing Graph
    Audio and MIDI routing is handled by jack
    Exposed via API to allow CRUD on jack routing by clients
*/

//!@todo Implement API access to routing graph


/*  Presets
    A preset is a configuration of an engine
    Preset access is implemented in Engine and Engine Class sections
*/


/*  Snapshots
    A snapshot is a full capture of the whole data model including:
    * instantiated engines
    * engine parameters
    * routing
    * mixer settings
    * chain configuration
    * etc.
*/

/** @brief  Get quantity of available snapshots
*   @retval uint32_t Quantity of available snapshots
*/
uint32_t getSnapshotCount();

/** @brief  Get name of snapshot
*   @param  snapshot Index of snapshot
*   @retval string Name of snapshot
*/
string getSnapshotNameByIndex(uint32_t snapshot);

/** @brief  Get name of snapshot
*   @param  path Full path and filename of snapshot
*   @retval string Name of snapshot
*/
string getSnapshotNameByPath(string path);

/** @brief  Set name of currently loaded snapshot
*   @param  snapshot Index of snapshot
*   @param  name New name for snapshot
*/
void setSnapshotName(uint32_t snapshot, string name);

/** @brief  Restore a snapshot from persistent storage
*   @param  path Full path and filename to snapshot file
*   @retval bool True on success
*/
bool loadSnapshot(string path);

/** @brief  Store current data model as a snapshot to persistent storage
*   @param  path Full path and filename to snapshot file
*   @retval bool True on success
*/
bool saveSnapshot(string path);


/*  Physical UI
    Access to switches, encoders, endless pots, LEDs, etc.
*/

/** @brief  Get quantity of switches
*   @retval uint32_t Quantity of switches
*/
uint32_t getSwitchCount();

/** @brief  Set quantity of switches
*   @param  uint32_t switches
*/
void setSwitchCount(uint32_t switches);

/** @brief  Get quantity of rotary encoders
*   @retval uint32_t Quantity of rotary encoders
*/
uint32_t getEncoderCount();

/** @brief  Set quantity of rotary encoders
*   @param  uint32_t encoders
*/
void setEncoderCount(uint32_t encoders);

/** @brief  Get quantity of endless potentiometers
*   @retval uint32_t Quantity of endless potentiometers
*   @todo   Is this same as encoder due to abstraction?
*/
uint32_t getEndlessPotCount();

/** @brief  Set quantity of endless potentiometers
*   @param  uint32_t pots
*   @todo   Is this same as encoder due to abstraction?
*/
void setEndlessPotCount(uint32_t pots);

/** @brief  Get switch state
*   @param  switch Index of switch
*   @retval bool True if switch closed
*/
bool isSwitchClosed(uint32_t switch);

/** @brief  Assign a MIDI command to a UI switch
*   @param  switch Index of switch
*   @param  event Type of MIDI event to send when switch closed
*   @param  channel MIDI channel
*   @param  command MIDI command
*   @param  value MIDI value
*   @retval int
*   @todo   Document return value. What is event type?
*/
int assignSwitchMidi(uint8_t switch, midi_event_type event, uint8_t midiChannel, uint8_t command, uint8_t value);

//!@todo Document get_zynswitch
unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus);

//-----------------------------------------------------------------------------
// Zynpot common API
//-----------------------------------------------------------------------------

/** @brief  Configure a zynpot
*   @param  zynpot Index of zynpot
*   @param  min Minimum value
*   @param  max Maximum value
*   @param  value Current / default value
*   @param  step Quantity of units to skip for each incremental change
*   @retval int
*   @todo   Describe return value
*/
int zynpotSetup(uint8_t zynpot, int32_t min, int32_t max, int32_t value, int32_t step);

/** @brief  Get current value of zynpot
*   @param  zynpot Index of zynpot
*   @retval int32_t Current value
*/
int32_t zynpotGetValue(uint8_t zynpot);

/** @brief  Set value of zynpot
*   @param  zynpot Index of zynpot
*   @param  value New value
*   @param  send True to send notification of new value
*   @todo   Describe return value
*/
int zynpotSetValue(uint8_t zynpot, int32_t value, bool send);

//!@todo   Document getZynpotFlag
uint8_t get_value_flag_zynpot(uint8_t zynpot);

/** @brief  Assign MIDI command to zynpot
*   @param  zynpot Index of zynpot
*   @param  channel MIDI channel
*   @param  command MIDI command
*   @todo   Describe return value
*/
int zynpotSetupMidi(uint8_t zynpot, uint8_t channel, uint8_t command);

/** @brief  Assign OSC command to zynpot
*   @param  zynpot Index of zynpot
*   @param  path OSC path
*   @todo   Describe return value
*/
int zynpotSetupOsc(uint8_t zynpot, char *path);


/*  Step Sequencer
    See zynseq.h
*/


/*  Real time messages
    Messages sent with low latency
*/

/** @brief  Send a MIDI command
*   @param  channel MIDI channel
*   @param  command MIDI command
*   @param  value MIDI value (ignored for 2 byte commands)
*/
void sendMidi(uint8_t channel, uint8_t command, uint8_t value);

/** @brief  Register callback to receive MIDI messages
*   @param  callback Pointer to callback accepting (uint8_t channel, uint8_t command, uint8_t value)
*   @param  channel MIDI channel (0..15, 0xFF for all)
*   @param  command MIDI command (0..127, 0xFF for all)
*   @param  min Minimum MIDI value (0..127, Default: 0, ignored for 2 byte commands)
*   @param  max Maximum MIDI value (0..127, Default: 127, ignored for 2 byte commands)
*/
void registerMidi(void* callback, uint8_t channel, uint8_t command, uint8_t min=0, uint8_t max=127);

/** @brief  Get transport state
*   @retval uint8_t Current transport state
*/
uint8_t getTransportState();

/** @brief  Set transport state
*   @param  state New transport state [STOPPED | ROLLING]
*/
void setTransportState(uint8_t state);

/** @brief  Get transport position
*   @retval uint32_t Transport position in ticks
*/
uint32_t getTransportPosition();

/** @brief  Set transport position
*   @param  position New position in ticks
*/
void setTransportPosition(uint32_t position);


/*  System messages
    Control and monitoring of core system
*/

/** @brief  Get time in seconds since boot
*   @retval uint32_t Quantity of seconds since boot
*/
uint32_t getUptime();

/** @brief  Get quantity of xruns since last reset
*   @retval uint32_t Quantity of xruns
*/
uint32_t getXruns();

/** @brief  Reset xrun counter
*/
void resetXruns();

/** @brief  Get quantity of high temperature alerts since last reset
*   @retval uint32_t Quantity of high temperature alerts
*/
uint32_t getHigTemperature()

/** @brief  Reset over voltage alert counter
*/
void resetHighTemperature();

/** @brief  Get quantity of under voltage alerts since last reset
*   @retval uint32_t Quantity of under voltage alerts
*/
uint32_t getUnderVoltage()

/** @brief  Reset under voltage alert counter
*/
void resetUnderVoltage();

/** @brief  Restart core
*   @note   Engines will be destroyed and recreated. Sequences will be stopped.
*/
void restartCore();

/** @brief  Shutdown and power off device
*/
void shutdown();

/** @brief  Restart device
*/
void reboot();

/** @brief  Send all note off message to all engines
*/
void panic();

/** @brief  Start audio recording
*   @param  filename Full path and filename for new recording (Default: Unique timestamped filename)
*/
void startAudioRecording(string filename="");

/** @brief  Start MIDI recording
*   @param  filename Full path and filename for new recording (Default: Unique timestamped filename)
*/
void startMidiRecording(string filename="");
