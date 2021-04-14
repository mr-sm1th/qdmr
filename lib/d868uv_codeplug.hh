#ifndef D868UV_CODEPLUG_HH
#define D868UV_CODEPLUG_HH

#include <QDateTime>

#include "codeplug.hh"
#include "signaling.hh"
#include "codeplugcontext.hh"

class Channel;
class DigitalContact;
class Zone;
class RXGroupList;
class ScanList;
class GPSSystem;


/** Represents the device specific binary codeplug for Anytone AT-D868UV radios.
 *
 * In contrast to many other code-plugs, the code-plug for Anytone radios are spread over a large
 * memory area. The amount of fragmentation of the codeplug is overwhelming.
 * For example, while channels are organized more or less nicely in continous banks, zones are
 * distributed throughout the entire code-plug. That is, the names of zones are located in a
 * different memory section that the channel lists. Some lists are defined though bit-masks others
 * by byte-masks. All bit-masks are positive, that is 1 indicates an enabled item while the
 * bit-mask for contacts is inverted.
 *
 * In general the code-plug is huge and complex. Moreover, the radio provides a huge amount of
 * options and features. To this end, reverse-engeneering this code-plug was a nightmare.
 *
 * More over, the binary code-plug file generate by the windows CPS does not directly relate to
 * the data being written to the radio. To this end the code-plug has been reverse-engineered
 * using wireshark to monitor the USB communication between the windows CPS (running in a vritual
 * box) and the device. The latter makes the reverse-engineering particularily cumbersome.
 *
 * @section d868uvcpl Codeplug structure within radio
 * <table>
 *  <tr><th colspan="3">Channels</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>024C1500</td> <td>000200</td>      <td>Bitmap of 4000 channels, default 0x00, 0x00 padded.</td></tr>
 *  <tr><td>00800000</td> <td>max. 002000</td> <td>Channel bank 0 of upto 128 channels, see @c channel_t 64 b each. </td></tr>
 *  <tr><td>00840000</td> <td>max. 002000</td> <td>Channel bank 1 of upto 128 channels.</td></tr>
 *  <tr><td>...</td>      <td>...</td>         <td>...</td></tr>
 *  <tr><td>00FC0000</td> <td>max. 000800</td> <td>Channel bank 32, upto 32 channels.</td></tr>
 *  <tr><td>00FC0800</td> <td>000040</td>      <td>VFO A settings, see @c channel_t.</td></tr>
 *  <tr><td>00FC0840</td> <td>000040</td>      <td>VFO B settings, see @c channel_t.</td></tr>
 *
 *  <tr><th colspan="3">Zones</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>024C1300</td> <td>000020</td>      <td>Bitmap of 250 zones.</td></tr>
 *  <tr><td>01000000</td> <td>max. 01f400</td> <td>250 zones channel lists of 250 16bit indices each.
 *    0-based, little endian, default/padded=0xffff. Offset between channel lists 0x200, size of each list 0x1f4.</td></tr>
 *  <tr><td>02540000</td> <td>max. 001f40</td> <td>250 Zone names.
 *    Each zone name is upto 16 ASCII chars long and gets 0-padded to 32b.</td></tr>
 *
 *  <tr><th colspan="3">Contacts</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>02600000</td> <td>max. 009C40</td> <td>Index list of valid contacts.
 *    10000 32bit indices, little endian, default 0xffffffff</td></tr>
 *  <tr><td>02640000</td> <td>000500</td>      <td>Contact bitmap, 10000 bit, inverted, default 0xff, 0x00 padded.</td></tr>
 *  <tr><td>02680000</td> <td>max. 0f4240</td> <td>10000 contacts, see @c contact_t.
 *    As each contact is 100b, they do not align with the 16b blocks being transferred to the device.
 *    Hence contacts are organized internally in groups of 4 contacts forming a "bank". </td></tr>
 *  <tr><td>04340000</td> <td>max. 013880</td> <td>DMR ID to contact index map, see @c contact_map_t.
 *    Sorted by ID, empty entries set to 0xffffffffffffffff.</td>
 *
 *  <tr><th colspan="3">Analog Contacts</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>02900000</td> <td>000080</td>      <td>Index list of valid ananlog contacts.</td></tr>
 *  <tr><td>02900100</td> <td>000080</td>      <td>Bytemap for 128 analog contacts.</td></tr>
 *  <tr><td>02940000</td> <td>max. 000180</td> <td>128 analog contacts. See @c analog_contact_t.
 *    As each analog contact is 24b, they do not align with the 16b transfer block-size. Hence
 *    analog contacts are internally organized in groups of 2. </td></tr>
 *
 *  <tr><th colspan="3">RX Group Lists</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>025C0B10</td> <td>000020</td>      <td>Bitmap of 250 RX group lists, default/padding 0x00.</td></tr>
 *  <tr><td>02980000</td> <td>max. 000120</td> <td>Grouplist 0, see @c grouplist_t.</td></tr>
 *  <tr><td>02980200</td> <td>max. 000120</td> <td>Grouplist 1</td></tr>
 *  <tr><td>...</td>      <td>...</td>         <td>...</td></tr>
 *  <tr><td>0299f200</td> <td>max. 000120</td> <td>Grouplist 250</td></tr>
 *
 *  <tr><th colspan="3">Scan lists</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>024C1340</td> <td>000020</td> <td>Bitmap of 250 scan lists.</td></tr>
 *  <tr><td>01080000</td> <td>000090</td> <td>Bank 0, Scanlist 1, see @c scanlist_t. </td></tr>
 *  <tr><td>01080200</td> <td>000090</td> <td>Bank 0, Scanlist 2</td></tr>
 *  <tr><td>...</td>      <td>...</td>    <td>...</td></tr>
 *  <tr><td>01081E00</td> <td>000090</td> <td>Bank 0, Scanlist 16</td></tr>
 *  <tr><td>010C0000</td> <td>000090</td> <td>Bank 1, Scanlist 17</td></tr>
 *  <tr><td>...</td>      <td>...</td>    <td>...</td></tr>
 *  <tr><td>01440000</td> <td>000090</td> <td>Bank 15, Scanlist 241</td></tr>
 *  <tr><td>...</td>      <td>...</td>    <td>...</td></tr>
 *  <tr><td>01441400</td> <td>000090</td> <td>Bank 15, Scanlist 250</td></tr>
 *
 *  <tr><th colspan="3">Radio IDs</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>024C1320</td> <td>000020</td>      <td>Bitmap of 250 radio IDs.</td></tr>
 *  <tr><td>02580000</td> <td>max. 001f40</td> <td>250 Radio IDs. See @c radioid_t.</td></tr>
 *
 *  <tr><th colspan="3">GPS</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>02501000</td> <td>000030</td> <td>GPS settings, see @c gps_setting_t.</td>
 *  <tr><td>02501100</td> <td>000030</td> <td>GPS message.</td>
 *
 *  <tr><th colspan="3">General Settings</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>02500000</td> <td>0000D0</td> <td>General settings, see @c general_settings_t.</td></tr>
 *  <tr><td>02500100</td> <td>000500</td> <td>Zone A & B channel list.</td></tr>
 *  <tr><td>02500500</td> <td>000100</td> <td>DTMF list</td></tr>
 *  <tr><td>02500600</td> <td>000030</td> <td>Power on settings</td></tr>
 *  <tr><td>024C2000</td> <td>0003F0</td> <td>List of 250 auto-repeater offset frequencies.
 *    32bit little endian frequency in 10Hz. I.e., 600kHz = 60000. Default 0x00000000, 0x00 padded.</td></tr>
 *
 *  <tr><th colspan="3">Messages</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>01640000</td> <td>max. 000100</td> <td>Some kind of linked list of messages.
 *    See @c message_list_t. Each entry has a size of 0x10.</td></tr>
 *  <tr><td>01640800</td> <td>000090</td>      <td>Bytemap of up to 100 valid messages.
 *    0x00=valid, 0xff=invalid, remaining 46b set to 0x00.</td></tr>
 *  <tr><td>02140000</td> <td>max. 000800</td> <td>Bank 0, Messages 1-8.
 *    Each message consumes 0x100b. See @c message_t. </td></tr>
 *  <tr><td>02180000</td> <td>max. 000800</td> <td>Bank 1, Messages 9-16</td></tr>
 *  <tr><td>...</td>      <td>...</td>         <td>...</td></tr>
 *  <tr><td>02440000</td> <td>max. 000800</td> <td>Bank 12, Messages 97-100</td></tr>
 *
 *  <tr><th colspan="3">Hot Keys</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>025C0000</td> <td>000100</td> <td>4 analog quick-call settings. See @c analog_quick_call_t.</td>
 *  <tr><td>025C0B00</td> <td>000010</td> <td>Status message bitmap.</td>
 *  <tr><td>025C0100</td> <td>000400</td> <td>Upto 32 status messages.
 *    Length unknown, offset 0x20. ASCII 0x00 terminated and padded.</td>
 *  <tr><td>025C0500</td> <td>000360</td> <td>18 hot-key settings, see @c hotkey_t</td></tr>
 *
 *  <tr><th colspan="3">Misc</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>024C1400</td> <td>000020</td> <td>Alarm setting, see @c analog_alarm_setting_t.</td></tr>
 *
 *  <tr><th colspan="3">FM Broadcast</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>        <th>Content</th></tr>
 *  <tr><td>02480210</td> <td>000020</td>      <td>Bitmap of 100 FM broadcast channels.</td></tr>
 *  <tr><td>02480000</td> <td>max. 000200</td> <td>100 FM broadcast channels. Encoded
 *    as 8-digit BCD little-endian in 100Hz. Filled with 0x00.</td></tr>
 *  <tr><td>02480200</td> <td>000010</td>      <td>FM broadcast VFO frequency. Encoded
 *    as 8-digit BCD little-endian in 100Hz. Filled with 0x00.</td></tr>
 *
 *  <tr><th colspan="3">Still unknown</th></tr>
 *  <tr><th>Start</th>    <th>Size</th>   <th>Content</th></tr>
 *  <tr><td>024C0000</td> <td>000020</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C0C80</td> <td>000010</td> <td>Unknown data, bitmap?, default 0x00.</td></tr>
 *  <tr><td>024C0D00</td> <td>000200</td> <td>Empty, set to 0x00?`</td></tr>
 *  <tr><td>024C1000</td> <td>0000D0</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C1100</td> <td>000010</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C1280</td> <td>000020</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C1440</td> <td>000030</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C1700</td> <td>000040</td> <td>Unknown, 8bit indices.</td></tr>
 *  <tr><td>024C1800</td> <td>000500</td> <td>Empty, set to 0x00?</td></tr>
 *  <tr><td>024C2400</td> <td>000030</td> <td>Unknown data.</td></tr>
 *  <tr><td>024C2600</td> <td>000010</td> <td>Unknown data.</td></tr>
 * </table>
 *
 * @ingroup d868uv */
class D868UVCodeplug : public CodePlug
{
  Q_OBJECT

public:
  /** Represents the actual channel encoded within the binary code-plug.
   *
   * Memmory layout of encoded channel (64byte):
   * @verbinclude d878uvchannel.txt
   */
  struct __attribute__((packed)) channel_t {
    /** Defines all possible channel modes, see @c channel_mode. */
    typedef enum {
      MODE_ANALOG    = 0,               ///< Analog channel.
      MODE_DIGITAL   = 1,               ///< Digital (DMR) channel.
      MODE_MIXED_A_D = 2,               ///< Mixed, transmit analog and receive digital.
      MODE_MIXED_D_A = 3                ///< Mixed, transmit digital and receive analog.
    } Mode;

    /** Defines all possible power settings.*/
    typedef enum {
      POWER_LOW = 0,                    ///< Low power, usually 1W.
      POWER_MIDDLE = 1,                 ///< Medium power, usually 2.5W.
      POWER_HIGH = 2,                   ///< High power, usually 5W.
      POWER_TURBO = 3                   ///< Higher power, usually 7W on VHF and 6W on UHF.
    } Power;

    /** Defines all band-width settings for analog channel.*/
    typedef enum {
      BW_12_5_KHZ = 0,                  ///< Narrow band-width (12.5kHz).
      BW_25_KHZ = 1                     ///< High band-width (25kHz).
    } Bandwidth;

    /** Defines all possible repeater modes. */
    typedef enum {
      RM_SIMPLEX = 0,                   ///< Simplex mode, that is TX frequency = RX frequency. @c tx_offset is ignored.
      RM_TXPOS = 1,                     ///< Repeater mode with positive @c tx_offset.
      RM_TXNEG = 2                      ///< Repeater mode with negative @c tx_offset.
    } RepeaterMode;

    /** Defines all possible squelch settings. */
    typedef enum {
      SQ_CARRIER = 0                    ///< Open squelch on carrier.
    } SquelchMode;

    /** Defines all possible admit criteria. */
    typedef enum {
      ADMIT_ALWAYS = 0,                 ///< Admit TX always.
      ADMIT_COLORCODE = 1,              ///< Admit TX on matching color-code.
      ADMIT_CH_FREE = 2,                   ///< Admit TX on channel free.
    } Admit;

    /** Defines all possible optional signalling settings. */
    typedef enum {
      OPTSIG_OFF = 0,                   ///< None.
      OPTSIG_DTMF = 1,                  ///< Use DTMF.
      OPTSIG_2TONE = 2,                 ///< Use 2-tone.
      OPTSIG_5TONE = 3                  ///< Use 5-tone.
    } OptSignaling;

    // Bytes 0-7
    uint32_t rx_frequency;              ///< RX Frequency, 8 digits BCD, big-endian.
    uint32_t tx_offset;                 ///< TX Offset, 8 digits BCD, big-endian, sign in repeater_mode.

    // Byte 8
    uint8_t channel_mode    : 2,        ///< Mode: Analog or Digital, see @c Mode.
      power                 : 2,        ///< Power: Low, Middle, High, Turbo, see @c Power.
      bandwidth             : 1,        ///< Bandwidth: 12.5 or 25 kHz, see @c Bandwidth.
      _unused8              : 1,        ///< Unused, set to 0.
      repeater_mode         : 2;        ///< Sign of TX frequency offset, see @c RepeaterMode.

    // Byte 9
    uint8_t rx_ctcss        : 1,        ///< CTCSS decode enable.
      rx_dcs                : 1,        ///< DCS decode enable.
      tx_ctcss              : 1,        ///< CTCSS encode enable.
      tx_dcs                : 1,        ///< DCS encode enable
      reverse               : 1,        ///< CTCSS phase-reversal.
      rx_only               : 1,        ///< TX prohibit.
      call_confirm          : 1,        ///< Call confirmation enable.
      talkaround            : 1;        ///< Talk-around enable.

    // Bytes 10-15
    uint8_t ctcss_transmit;             ///< TX CTCSS tone, 0=62.5, 50=254.1, 51=custom CTCSS tone.
    uint8_t ctcss_receive;              ///< RX CTCSS tone: 0=62.5, 50=254.1, 51=custom CTCSS tone.
    uint16_t dcs_transmit;              ///< TX DCS code: 0=D000N, 511=D777N, 512=D000I, 1023=D777I, DCS code-number in octal, little-endian.
    uint16_t dcs_receive;               ///< RX DCS code: 0=D000N, 511=D777N, 512=D000I, 1023=D777I, DCS code-number in octal, little-endian.

    // Bytes 16-19
    uint16_t custom_ctcss;              ///< Custom CTCSS tone frequency: 0x09cf=251.1, 0x0a28=260, big-endian.
    uint8_t tone2_decode;               ///< 2-Tone decode: 0x00=1, 0x0f=16
    uint8_t _unused19;                  ///< Unused, set to 0.

    // Bytes 20-23
    uint32_t contact_index;             ///< Contact index, zero-based, little-endian.

    // Byte 24
    uint8_t id_index;                   ///< Index to radio ID table.

    // Byte 25
    uint8_t _unused25_0     : 4,        ///< Unused set to 0.
      squelch_mode          : 1,        ///< Squelch mode, see @c SquelchMode.
      _unused25_2           : 3;        ///< Unused, set to 0.

    // Byte 26
    uint8_t tx_permit       : 2,        ///< TX permit, see @c Admit.
      _unused26_1           : 2,        ///< Unused, set to 0.
      opt_signal            : 2,        ///< Optional signaling, see @c OptSignaling.
      _unused26_2           : 2;        ///< Unused, set to 0.

    // Bytes 27-31
    uint8_t scan_list_index;            ///< Scan list index, 0xff=None, 0-based.
    uint8_t group_list_index;           ///< RX group-list, 0xff=None, 0-based.
    uint8_t id_2tone;                   ///< 2-Tone ID, 0=1, 0x17=24.
    uint8_t id_5tone;                   ///< 5-Tone ID, 0=1, 0x63=100.
    uint8_t id_dtmf;                    ///< DTMF ID, 0=1, 0x0f=16.

    // Byte 32
    uint8_t color_code;                 ///< Color code, 0-15

    // Byte 33
    uint8_t slot2           : 1,        ///< Timeslot, 0=TS1, 1=TS2.
      sms_confirm           : 1,        ///< Send SMS confirmation, 0=off, 1=on.
      simplex_tdma          : 1,        ///< Simplex TDMA enabled.
      _unused33_2           : 1,        ///< Unused, set to 0.
      tdma_adaptive         : 1,        ///< TDMA adaptive enable.
      rx_gps                : 1,        ///< Receive digital GPS messages.
      enh_encryption        : 1,        ///< Enable enhanced encryption.
      work_alone            : 1;        ///< Work alone, 0=off, 1=on.

    // Byte 34
    uint8_t _unused34;                  ///< Unused, set to 0.

    // Bytes 35-51
    uint8_t name[16];                   ///< Channel name, ASCII, zero filled.
    uint8_t _pad51;                     ///< Pad byte, set to 0.

    // Byte 52
    uint8_t rangeing        : 1,        ///< Rangeing enable.
      through_mode          : 1,        ///< Through-mode enabled.
      data_ack_forbit       : 1,        ///< Data ACK forbit.
      _unused52             : 5;        ///< Unused, set to 0.

    // Byte 53
    uint8_t aprs_report;                ///< Enable positioning, 0x00=off, 0x01 on.
    uint8_t gps_system;                 ///< Index of DMR GPS report system, 0-7;

    // Bytes 55-63
    uint8_t _unused55;                  ///< Unused set to 0.
    uint8_t _unused56;                  ///< Unused set to 0
    uint8_t _unused57;                  ///< Unused set to 0.
    uint8_t dmr_encryption;             ///< Digital encryption, 1-32, 0=off.
    uint8_t multiple_keys   : 1,        ///< Enable multiple keys.
      random_key            : 1,        ///< Enable random key.
      sms_forbid            : 1,        ///< Forbit SMS tramsission.
      _unused59_3           : 5;        ///< Unused, set to 0.
    uint32_t _unused60;                 ///< Unused, set to 0.

    /** Constructor, also clears the struct. */
    channel_t();

    /** Clears and invalidates the channel. */
    void clear();

    /** Returns @c true if the channel is valid. */
    bool isValid() const;

    /** Returns the RX frequency in MHz. */
    double getRXFrequency() const;
    /** Sets the RX frequency in MHz. */
    void setRXFrequency(double f);

    /** Returns the TX frequency in MHz. */
    double getTXFrequency() const;
    /** Sets the TX frequency in MHz.
     * @note As the TX frequency is stored as difference to the RX frequency, the RX frequency
     * should be set first. */
    void setTXFrequency(double f);

    /** Returns the name of the radio. */
    QString getName() const;
    /** Sets the name of the radio. */
    void setName(const QString &name);

    /** Returns the RX CTCSS/DCS tone. */
    Signaling::Code getRXTone() const;
    /** Sets the RX CTCSS/DCS tone. */
    void setRXTone(Signaling::Code code);
    /** Returns the TX CTCSS/DCS tone. */
    Signaling::Code getTXTone() const;
    /** Sets the TX CTCSS/DCS tone. */
    void setTXTone(Signaling::Code code);

    /** Constructs a generic @c Channel object from the codeplug channel. */
    Channel *toChannelObj() const;
    /** Links a previously constructed channel to the rest of the configuration. */
    bool linkChannelObj(Channel *c, const CodeplugContext &ctx) const;
    /** Initializes this codeplug channel from the given generic configuration. */
    void fromChannelObj(const Channel *c, const Config *conf);
  };

  /** Represents a digital contact within the binary codeplug.
   *
   * Memmory layout of encoded contact (100byte):
   * @verbinclude d878uvcontact.txt
   */
  struct __attribute__((packed)) contact_t {
    /** Possible call types. */
    enum CallType : uint8_t {
      CALL_PRIVATE = 0,                 ///< Private call.
      CALL_GROUP = 1,                   ///< Group call.
      CALL_ALL = 2                      ///< All call.
    };

    /** Possible ring-tone types. */
    enum AlertType : uint8_t {
      ALERT_NONE = 0,                   ///< Alert disabled.
      ALERT_RING = 1,                   ///< Ring tone.
      ALERT_ONLINE = 2                  ///< WTF?
    };

    // Byte 0
    CallType type;                      ///< Call Type: Group Call, Private Call or All Call.
    // Bytes 1-16
    uint8_t name[16];                   ///< Contact Name max 16 ASCII chars 0-terminated.
    // Bytes 17-34
    uint8_t _unused17[18];              ///< Unused, set to 0.
    // Bytes 35-38
    uint32_t id;                        ///< Call ID, BCD coded 8 digits, big-endian.
    // Byte 39
    AlertType call_alert;               ///< Call Alert. One of None, Ring, Online Alert.
    // Bytes 40-99
    uint8_t _unused40[60];              ///< Unused, set to 0.

    /** Constructs a new and empty contact. */
    contact_t();

    /** Clears the contact. */
    void clear();

    /** Returns @c true if the contact is valid. */
    bool isValid() const;

    /** Retruns the call type. */
    DigitalContact::Type getType() const;
    /** Sets the call type. */
    void setType(DigitalContact::Type type);

    /** Returns the name of the contact. */
    QString getName() const;
    /** Sets the name of the contact. */
    void setName(const QString &name);

    /** Returns the number of the contact. */
    uint32_t getId() const;
    /** Set the number of the contact. */
    void setId(uint32_t id);

    /** Retunrs @c true if a ring-tone is enabled for this contact. */
    bool getAlert() const;
    /** Enables/disables a ring-tone for this contact. */
    void setAlert(bool enable);

    /** Assembles a @c DigitalContact from this contact. */
    DigitalContact *toContactObj() const;
    /** Constructs this contact from the give @c DigitalContact. */
    void fromContactObj(const DigitalContact *contact);
  };

  /** Represents an ananlog contact within the binary codeplug.
   *
   * Memmory layout of encoded analog contact (48byte):
   * @verbinclude d878uvanalogcontact.txt
   */
  struct __attribute__((packed)) analog_contact_t {
    uint8_t number[7];                  ///< Number encoded as BCD big-endian. Although it can hold
                                        /// up to 14 digits, more than 8 will crash the manufacturer CPS.
    uint8_t digits;                     ///< Number of digits.
    uint8_t name[15];                   ///< Name, ASCII, upto 15 chars, 0-terminated & padded.
    uint8_t pad47;                      ///< Pad byte set to 0x00.
  };

  /** Represents the actual RX group list encoded within the binary code-plug.
   *
   * Memmory layout of encoded group list (288byte):
   * @verbinclude d878uvgrouplist.txt
   */
  struct __attribute__((packed)) grouplist_t {
    // Bytes 0-255
    uint32_t member[64];                ///< Contact indices, 0-based, little-endian, empty=0xffffffff.
    // Bytes 256-287
    uint8_t name[16];                   ///< Group-list name, up to 16 x ASCII, 0-terminated.
    uint8_t unused[16];                 ///< Unused, set to 0.

    /** Constructs an empty group list. */
    grouplist_t();

    /** Clears this group list. */
    void clear();
    /** Returns @c true if the group list is valid. */
    bool isValid() const;

    /** Returns the name of the group list. */
    QString getName() const;
    /** Sets the name of the group list. */
    void setName(const QString &name);

    /** Constructs a new @c RXGroupList from this group list.
     * None of the members are added yet. Call @c linkGroupList
     * to do that. */
    RXGroupList *toGroupListObj() const;
    /** Populates the @c RXGroupList from this group list. The CodeplugContext
     * is used to map the member indices. */
    bool linkGroupList(RXGroupList *lst, const CodeplugContext &ctx);
    /** Constructs this group list from the given @c RXGroupList. */
    void fromGroupListObj(const RXGroupList *lst, const Config *conf);
  };

  /** Represents a scan list within the binary codeplug.
   *
   * Memmory layout of encoded scanlist (144byte):
   * @verbinclude d878uvscanlist.txt
   */
  struct __attribute__((packed)) scanlist_t {
    /** Defines all possible priority channel selections. */
    typedef enum {
      PRIO_CHAN_OFF = 0,                ///< Off.
      PRIO_CHAN_SEL1 = 1,               ///< Priority Channel Select 1.
      PRIO_CHAN_SEL2 = 2,               ///< Priority Channel Select 2.
      PRIO_CHAN_SEL12 = 3               ///< Priority Channel Select 1 + Priority Channel Select 2.
    } PriChannel;

    /** Defines all possible reply channel selections. */
    enum RevertChannel : uint8_t {
      REVCH_SELECTED = 0,               ///< Selected.
      REVCH_SEL_TB = 1,                 ///< Selected + TalkBack.
      REVCH_PRIO_CH1 = 2,               ///< Priority Channel Select 1.
      REVCH_PRIO_CH2 = 3,               ///< Priority Channel Select 2.
      REVCH_LAST_CALLED = 4,            ///< Last Called.
      REVCH_LAST_USED = 5,              ///< Last Used.
      REVCH_PRIO_CH1_TB = 6,            ///< Priority Channel Select 1 + TalkBack.
      REVCH_PRIO_CH2_TB = 7             ///< Priority Channel Select 2 + TalkBack.
    };

    uint8_t _unused0000;                ///< Unused, set to 0.
    uint8_t prio_ch_select;             ///< Priority Channel Select, default = PRIO_CHAN_OFF.
    uint16_t priority_ch1;              ///< Priority Channel 1: 0=Current Channel, index+1, little endian, 0xffff=Off.
    uint16_t priority_ch2;              ///< Priority Channel 2: 0=Current Channel, index+1, little endian, 0xffff=Off.
    uint16_t look_back_a;               ///< Look Back Time A, sec*10, little endian, default 0x000f.
    uint16_t look_back_b;               ///< Look Back Time B, sec*10, little endian, default 0x0019.
    uint16_t dropout_delay;             ///< Dropout Delay Time, sec*10, little endian, default 0x001d.
    uint16_t dwell;                     ///< Dwell Time, sec*10, little endian, default 0x001d.
    RevertChannel revert_channel;       ///< Revert Channel, see @c RevertChannel, default REVCH_SELECTED.
    uint8_t name[16];                   ///< Scan List Name, ASCII, 0-terminated.
    uint8_t _pad001e;                   ///< Pad byte, set to 0x00.
    // Bytes 0x20
    uint16_t member[50];                ///< Channels indices, 0-based, little endian, 0xffff=empty
    uint8_t _unused0084[12];            ///< Unused, set to 0.

    /** Constructor. */
    scanlist_t();
    /** Clears the scan list. */
    void clear();
    /** Returns the name of the scan list. */
    QString getName() const;
    /** Sets the name of the scan list. */
    void setName(const QString &name);

    /** Constructs a ScanList object from this definition. This only sets the properties of
     * the scan list. To associate all members with the scan list object, call @c linkScanListObj. */
    ScanList *toScanListObj();
    /** Links all channels (members and primary channels) with the given scan-list object. */
    void linkScanListObj(ScanList *lst, CodeplugContext &ctx);
    /** Constructs the binary representation from the give config. */
    bool fromScanListObj(ScanList *lst, Config *config);
  };

  /** Represents an entry of the radio ID table within the binary codeplug.
   *
   * Memmory layout of encoded radio ID (32byte):
   * @verbinclude d878uvradioid.txt
   */
  struct __attribute__((packed)) radioid_t {
    // Bytes 0-3.
    uint32_t id;                        ///< Up to 8 BCD digits in little-endian.
    // Byte 4.
    uint8_t _unused4;                   ///< Unused, set to 0.
    // Bytes 5-20
    uint8_t name[16];                   ///< Name, up-to 16 ASCII chars, 0-terminated.
    // Bytes 21-31
    uint8_t _unused21[11];              ///< Unused, set to 0.

    /** Constructs an empty radio ID entry. */
    radioid_t();
    /** Clears the radio ID enty. */
    void clear();
    /** Returns @c true if the radio ID entry is valid. */
    bool isValid() const;

    /** Returns the name of the radio ID entry. */
    QString getName() const;
    /** Sets the name of the radio ID entry. */
    void setName(const QString name);

    /** Returns the radio ID of the entry. */
    uint32_t getId() const;
    /** Sets the radio ID of the entry. */
    void setId(uint32_t num);
  };

  /** Represents the general config of the radio within the binary codeplug.
   *
   * At 0x02500000, size 0x0d0. */
  struct __attribute__((packed)) general_settings_base_t {
    uint8_t _unknown0000[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0010[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0020[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0030[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0040[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0050[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0060[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0070[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0080[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown0090[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown00a0[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown00b0[0x10];    ///< For now a big unknown settings block.
    uint8_t _unknown00c0[0x10];    ///< For now a big unknown settings block.

    /** Encodes the general settings. */
    void fromConfig(Config *config, const Flags &flags);
    /** Updates the abstract config from general settings. */
    void updateConfig(Config *config);
  };

  /** At 0x02500100, size 0x400. */
  struct __attribute__((packed)) zone_channels_t {
    // Bytes 0x000-0x1ff
    uint16_t zone_a_channel[250];  ///< Channel index whithin each zone for channel A. Index 0-based litte-endian.
    uint8_t  _pad01f4[12];         ///< pad bytes.
    // Bytes 0x200-0x3ff
    uint16_t zone_b_channel[250];  ///< Channel index whithin each zone for channel B. Index 0-based litte-endian.
    uint8_t  _pad03f4[12];         ///< pad bytes.
  };

  /** At 0x02500500, size 0x100. */
  struct __attribute__((packed)) dtmf_numbers_t {
    // Bytes 0x000-0x0ff
    uint8_t  numbers[16][16];      ///< DTMF numbers, unused set to 0xff
  };

  /** At 0x02500600, size 0x030. */
  struct __attribute__((packed)) boot_settings_t {
    // Bytes 0x600-0x61f
    uint8_t intro_line1[16];       ///< Intro line 1, up to 14 ASCII characters, 0-terminated.
    uint8_t intro_line2[16];       ///< Intro line 2, up to 14 ASCII characters, 0-terminated.

    // Bytes 0x620-0x62f
    uint8_t password[16];          ///< Boot password, up to 8 ASCII digits, 0-terminated.

    /** Clears the general setting. */
    void clear();

    /** Returns the first intro-line. */
    QString getIntroLine1() const;
    /** Sets the first intro-line. */
    void setIntroLine1(const QString line);

    /** Returns the second intro-line. */
    QString getIntroLine2() const;
    /** Sets the second intro-line. */
    void setIntroLine2(const QString line);

    /** Updates the general settings from the given abstract configuration. */
    void fromConfig(const Config *config, const Flags &flags);
    /** Updates the abstract configuration from this general settings. */
    void updateConfig(Config *config);
  };

  /** GPS settings within the codeplug. */
  struct __attribute__((packed)) gps_settings_t {
    enum Power: uint8_t {
      POWER_LOW = 0,
      POWER_MID = 1,
      POWER_HIGH = 2,
      POWER_TURBO = 3
    };

    enum CallType: uint8_t {
      PRIVATE_CALL = 0,
      GROUP_CALL = 1,
      ALL_CALL = 2
    };

    enum TimeSlot: uint8_t {
      TIMESLOT_SAME = 0,
      TIMESLOT_1    = 1,
      TIMESLOT_2    = 2
    };

    uint8_t manual_tx_intervall;   ///< Specifies the manual transmit intervall in seconds [0,255].
    uint8_t auto_tx_intervall;     ///< Specifies the automatic transmit intervall in multiples of
                                   ///  15 seconds. The time is then defined as t=45+15*n, 0=off.

    uint8_t enable_fixed_location; ///< If 0x01, enables fixed location beacon, default 0x00=off.
    uint8_t lat_deg;               ///< Latitude in degree.
    uint8_t lat_min;               ///< Latitude minutes.
    uint8_t lat_sec;               ///< Latitude seconds (1/100th of a minute).
    uint8_t north_south;           ///< North or south flag, north=0, south=1.
    uint8_t lon_deg;               ///< Longitude in degree.
    uint8_t lon_min;               ///< Longitude in minutes.
    uint8_t lon_sec;               ///< Longitude in seconds (1/100th of a minute).
    uint8_t east_west;             ///< East or west flag, east=0, west=1.

    Power transmit_power;          ///< Specifies the transmit power.

    uint16_t channel_idxs[8];      ///< Specifies the 8 transmit channels for GPS information, litte endian.
                                   ///  Default 0x0fa1=VFO B, 0x0fa2=current channel.

    uint32_t target_id;            ///< Specifies the target DMR ID to send the APRS info to.
                                   ///  Encoded as big-endian 32bit BCD (8 digits).

    CallType call_type;            ///< Specifies the call type used to transmit the APRS info.
    TimeSlot timeslot;             ///< Specifies the time slot to use default=0 (same as selected channel).
    uint8_t _unused0022[14];       ///< Unused, set to 0.

    void clear();

    uint8_t getManualTXIntervall() const;
    void setManualTXIntervall(uint8_t period);

    uint16_t getAutomaticTXIntervall() const;
    void setAutomaticTXIntervall(uint16_t period);

    bool isFixedLocation() const;
    double getFixedLon() const;
    double getFixedLat() const;
    void setFixedLocation(double lat, double lon);

    Channel::Power getTransmitPower() const;
    void setTransmitPower(Channel::Power power);

    bool isChannelSelected(uint8_t i) const;
    bool isChannelVFOA(uint8_t i) const;
    bool isChannelVFOB(uint8_t i) const;
    uint16_t getChannelIndex(uint8_t i) const;
    void setChannelIndex(uint8_t i, uint16_t idx);
    void setChannelSelected(uint8_t i);
    void setChannelVFOA(uint8_t i);
    void setChannelVFOB(uint8_t i);

    uint32_t getTargetID() const;
    void setTargetID(uint32_t id);

    DigitalContact::Type getTargetType() const;
    void setTargetType(DigitalContact::Type type);
  };

  /** Some weird linked list of valid message indices.
   *
   * Memmory layout of encoded radio ID (16byte):
   * @verbinclude d878uvmessagelist.txt
   */
  struct __attribute__((packed)) message_list_t {
    uint8_t _unknown0[2];          ///< Unused, set to 0x00.
    uint8_t _next_index;           ///< Index of next message, 0xff=EOL.
    uint8_t _current_index;        ///< Index of this message.
    uint8_t _unknown4[12];         ///< Unused, set to 0x00.
  };

  /** Represents a prefabricated SMS message within the binary codeplug.
   *
   * Memmory layout of encoded radio ID (256byte):
   * @verbinclude d878uvmessage.txt
   */
  struct __attribute__((packed)) message_t {
    char text[99];                 ///< Up to 99 ASCII chars, 0-padded.
    uint8_t _unused100[157];       ///< Unused, set to 0.
  };

  /** Represents analog quick-call settings within the binary code-plug.
   * Size is 2 bytes. */
  struct __attribute__((packed)) analog_quick_call_t {
    /** Analog quick-call types. */
    typedef enum {
      AQC_None = 0,                ///< None, quick-call disabled.
      AQC_DTMF = 1,                ///< DTMF call.
      AQC_2TONE = 2,               ///< 2-tone call.
      AQC_5TONE = 3                ///< 5-tone call
    } Type;

    uint8_t call_type;             ///< Type of quick call, see @c Type.
    uint8_t call_id_idx;           ///< Index of whom to call. 0xff=none.
  };

  /** Represents hot-key settings. */
  struct __attribute__((packed)) hotkey_t {
    /** Hot-key types. */
    typedef enum {
      HOTKEY_CALL = 0,             ///< Perform a call.
      HOTKEY_MENU = 1              ///< Show a menu item.
    } Type;

    /** Possible menu hot-key settings. */
    typedef enum {
      HOTKEY_MENU_SMS = 1,         ///< Show SMS menu.
      HOTKEY_MENU_NEW_SMS = 2,     ///< Create new SMS.
      HOTKEY_MENU_HOT_TEXT = 3,    ///< Send a hot-text.
      HOTKEY_MENU_RX_SMS = 4,      ///< Show SMS inbox.
      HOTKEY_MENU_TX_SMS = 5,      ///< Show SMS outbox.
      HOTKEY_MENU_CONTACT = 6,     ///< Show contact list.
      HOTKEY_MENU_MANUAL_DIAL = 7, ///< Show manual dial.
      HOTKEY_MENU_CALL_LOG = 8     ///< Show call log.
    } MenuItem;

    /** Possible hot-key calls. */
    typedef enum {
      HOTKEY_CALL_ANALOG = 0,      ///< Perform an analog call.
      HOTKEY_CALL_DIGITAL = 1      ///< Perform a digital call.
    } CallType;

    /** Possible digital call types. */
    typedef enum {
      HOTKEY_DIGI_CALL_OFF = 0xff, ///< Call disabled.
      HOTKEY_GROUP_CALL = 0,       ///< Perform a group call.
      HOTKEY_PRIVATE_CALL = 1,     ///< Perform private call.
      HOTKEY_ALLCALL = 2,          ///< Perform all call.
      HOTKEY_HOT_TEXT = 3,         ///< Send a text message.
      HOTKEY_CALL_TIP = 4,         ///< Send a call tip (?).
      HOTKEY_STATE = 5             ///< Send a state message.
    } DigiCallType;

    uint8_t type;                  ///< Type of the hot-key, see @c Type.
    uint8_t meun_item;             ///< The menu item if type=HOTKEY_MENU, see @c MenuItem.
    uint8_t call_type;             ///< The call type if type=HOTKEY_CALL, see @c CallType.
    uint8_t digi_call_type;        ///< The digital call variant if call_type=HOTKEY_CALL_ANALOG, see @c DigiCallType.
    uint32_t call_obj;             ///< 32bit index of contact to call, little-endian. 0xffffffff=off.
                                   /// If call_type=HOTKEY_CALL_ANALOG, index of analog quick call.
                                   /// If call_type=HOTKEY_CALL_DIGIAL, index of any contact (see @c contact_t).
    uint8_t content;               ///< Content index, 0xff=none.
                                   /// If digi_call_type=HOTKEY_HOT_TEXT, index of message.
                                   /// If digi_call_type=HOTKEY_STATE, index of state message.
    uint8_t _unused9[39];          ///< Unused, set to 0x00.
  };


  /** Binary representation of the analog alarm settings.
   * Size 0x6 bytes. */
  struct __attribute__((packed)) analog_alarm_setting_t {
    /** Possible alarm types. */
    typedef enum {
      ALARM_AA_NONE = 0,           ///< No alarm at all.
      ALARM_AA_TX_AND_BG = 1,      ///< Transmit and background.
      ALARM_AA_TX_AND_ALARM = 2,   ///< Transmit and alarm
      ALARM_AA_BOTH = 3,           ///< Both?
    } Action;

    /** Possible alarm signalling types. */
    typedef enum {
      ALARM_ENI_NONE = 0,          ///< No alarm code signalling.
      ALARM_ENI_DTMF = 1,          ///< Send alarm code as DTMF.
      ALARM_ENI_5TONE = 2          ///< Send alarm code as 5-tone.
    } ENIType;

    uint8_t action;                ///< Action to take, see @c Action.
    uint8_t eni_type;              ///< ENI type, see @c ENIType.
    uint8_t emergency_id_idx;      ///< Emergency ID index, 0-based.
    uint8_t time;                  ///< Alarm time in seconds, default 10.
    uint8_t tx_dur;                ///< TX duration in seconds, default 10.
    uint8_t rx_dur;                ///< RX duration in seconds, default 60.
  };

  /** Represents an entry in the DMR ID -> contact map within the binary code-plug. */
  struct __attribute__((packed)) contact_map_t {
    uint32_t id_group;             ///< Combined ID and group-call flag. The ID is encoded in
                                   /// BCD in big-endian, shifted to the left by one bit. Bit 0 is
                                   /// then the group-call flag. The cobined id and flag is then
                                   /// stored in little-endian.
    uint32_t contact_index;        ///< Index to contact, 32bit little endian.

    /** Constructor, clears the map entry. */
    contact_map_t();
    /** Clears the map entry. */
    void clear();
    /** Returns @c true if this entry is valid. */
    bool isValid() const;
    /** Returns @c true if the entry is a group-call contact. */
    bool isGroup() const;
    /** Returns the DMR ID of the entry. */
    uint32_t ID() const;
    /** Sets the ID and group-call flag of the entry. */
    void setID(uint32_t id, bool group);
    /** Returns the contact index of the entry. */
    uint32_t index() const;
    /** Sets the contact index of the entry. */
    void setIndex(uint32_t index);
  };


public:
  /** Empty constructor. */
  explicit D868UVCodeplug(QObject *parent = nullptr);

  /** Clears and resets the complete codeplug to some default values. */
  virtual void clear();

  /** Sets all bitmaps for the given config. */
  virtual void setBitmaps(Config *config);

  /** Allocate all code-plug elements that must be downloaded for decoding. All code-plug elements
   * within the radio that are not represented within the common Config are omitted. */
  virtual void allocateForDecoding();
  /** Allocate all code-plug elements that must be written back to the device to maintain a working
   * codeplug. These elements might be updated during encoding. */
  virtual void allocateUpdated();
  /** Allocate all code-plug elements that are defined through the common Config. */
  virtual void allocateForEncoding();

  /** Decodes the binary codeplug and stores its content in the given generic configuration. */
	bool decode(Config *config);

  /** Encodes the given generic configuration as a binary codeplug. */
  bool encode(Config *config, const Flags &flags = Flags());

protected:
  /** Decodes the downloaded codeplug. */
  virtual bool decode(Config *config, CodeplugContext &ctx);

  /** Allocate channels from bitmap. */
  virtual void allocateChannels();
  /** Encode channels into codeplug. */
  virtual bool encodeChannels(Config *config, const Flags &flags);
  /** Create channels from codeplug. */
  virtual bool createChannels(Config *config, CodeplugContext &ctx);
  /** Link channels. */
  virtual bool linkChannels(Config *config, CodeplugContext &ctx);

  /** Allocate VFO settings. */
  virtual void allocateVFOSettings();

  /** Allocate contacts from bitmaps. */
  virtual void allocateContacts();
  /** Encode contacts into codeplug. */
  virtual bool encodeContacts(Config *config, const Flags &flags);
  /** Create contacts from codeplug. */
  virtual bool createContacts(Config *config, CodeplugContext &ctx);

  /** Allocate analog contacts from bitmaps. */
  virtual void allocateAnalogContacts();

  /** Allocate radio IDs from bitmaps. */
  virtual void allocateRadioIDs();
  /** Encode radio ID into codeplug. */
  virtual bool encodeRadioID(Config *config, const Flags &flags);
  /** Set radio ID from codeplug. */
  virtual bool setRadioID(Config *config, CodeplugContext &ctx);

  /** Allocates RX group lists from bitmaps. */
  virtual void allocateRXGroupLists();
  /** Encode RX group lists into codeplug. */
  virtual bool encodeRXGroupLists(Config *config, const Flags &flags);
  /** Create RX group lists from codeplug. */
  virtual bool createRXGroupLists(Config *config, CodeplugContext &ctx);
  /** Link RX group lists. */
  virtual bool linkRXGroupLists(Config *config, CodeplugContext &ctx);

  /** Allocate zones from bitmaps. */
  virtual void allocateZones();
  /** Encode zones into codeplug. */
  virtual bool encodeZones(Config *config, const Flags &flags);
  /** Create zones from codeplug. */
  virtual bool createZones(Config *config, CodeplugContext &ctx);
  /** Link zones. */
  virtual bool linkZones(Config *config, CodeplugContext &ctx);

  /** Allocate scanlists from bitmaps. */
  virtual void allocateScanLists();
  /** Encode scan lists into codeplug. */
  virtual bool encodeScanLists(Config *config, const Flags &flags);
  /** Create scan lists from codeplug. */
  virtual bool createScanLists(Config *config, CodeplugContext &ctx);
  /** Link scan lists. */
  virtual bool linkScanLists(Config *config, CodeplugContext &ctx);

  /** Allocates general settings memory section. */
  virtual void allocateGeneralSettings();
  /** Encodes the general settings section. */
  virtual bool encodeGeneralSettings(Config *config, const Flags &flags);
  /** Decodes the general settings section. */
  virtual bool decodeGeneralSettings(Config *config);

  /** Allocates zone channel list memory section. */
  virtual void allocateZoneChannelList();

  /** Allocates DTMF number list memory section. */
  virtual void allocateDTMFNumbers();

  /** Allocates boot settings memory section. */
  virtual void allocateBootSettings();
  /** Encodes the boot settings section. */
  virtual bool encodeBootSettings(Config *config, const Flags &flags);
  /** Decodes the boot settings section. */
  virtual bool decodeBootSettings(Config *config);

  /** Allocates GPS settings memory section. */
  virtual void allocateGPSSystems();
  /** Encodes the GPS settings section. */
  virtual bool encodeGPSSystems(Config *config, const Flags &flags);
  /** Create GPS systems from codeplug. */
  virtual bool createGPSSystems(Config *config, CodeplugContext &ctx);
  /** Link GPS systems. */
  virtual bool linkGPSSystems(Config *config, CodeplugContext &ctx);

  /** Allocate refab SMS messages. */
  virtual void allocateSMSMessages();
  /** Allocates hot key settings memory section. */
  virtual void allocateHotKeySettings();
  /** Allocates repeater offset settings memory section. */
  virtual void allocateRepeaterOffsetSettings();
  /** Allocates alarm settings memory section. */
  virtual void allocateAlarmSettings();
  /** Allocates FM broadcast settings memory section. */
  virtual void allocateFMBroadcastSettings();

  /** Internal used function to encode CTCSS frequencies. */
  static uint8_t ctcss_code2num(Signaling::Code code);
  /** Internal used function to decode CTCSS frequencies. */
  static Signaling::Code ctcss_num2code(uint8_t num);
};

#endif // D868UVCODEPLUG_HH