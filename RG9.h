#include "esphome.h"

class RG9Component : public PollingComponent, public UARTDevice, public Sensor, public CustomAPIDevice {
 public:
  RG9Component(UARTComponent *parent) : PollingComponent(1000), UARTDevice(parent) {}

  uint8_t     line[64];
  int         lastPoll;
  uint16_t    pollCount;
  uint16_t    failCount;
    
  bool readLine()
    {
      bzero(line, sizeof(line));
      unsigned int i = 0;
      unsigned int n = 0;
      while ((n = available()) == 0) {
        if (i++ > 1000) return false;
      }

      if (n >= sizeof(line)) n = sizeof(line)-1;
      read_array(line, n);
      for (unsigned int i = 0; i < n; i++) {
        if (line[i] == '\0') line[i] = '\n';
      }
      char tmp[128];
      sprintf(tmp, "%d: %s", n, line);
      ESP_LOGD("RG9", (char*) tmp);
      
      return true;
    }
    
  int parseLine()
    {
      int rc = -1;
      uint8_t *p = line;
      while (1) {
        while (*p++ != 'R') {
          if (*p == '\0') return rc;
        }
        while (isspace(*p)) p++;
     
        int val = *p - '0';
        if (0 <= val && val <= 9) rc = val;
      }
      return rc;
    }
   
  float get_setup_priority() const override
    {
      return esphome::setup_priority::LATE;
    }
 
  void setup() override
    {
      lastPoll  = -1;
      pollCount = 0;
      failCount = 0;
      register_service(&RG9Component::on_reset, "reset");
    }
  
  void on_reset()
    {
      pollCount = 50;
      ESP_LOGD("RG9", "Resetting RG-9...");
    }
  
  void update() override
    {
      if (pollCount++ == 55) {
        // Reset the device and put in continuous mode approx every day
        // 65536 seconds is 18.2 hours...
        write_str("K\n");
        flush();
        return;
      }
      if (pollCount == 60) {
        write_str("P\n");
      }
      write_str("R\n");
      flush();

      // Response: "R [0-9]\n"
      if (readLine()) {
        int state = parseLine();
        if (state >= 0) {
          polledState(state);
          return;
        }
      }
    
      // If we can't read the sensor for an hour, something is wrong
      if (failCount++ > 3600) {
        lastPoll = -1;
        polledState(-1);
      }
    }
    
  void polledState(int state)
    {
      failCount = 0;
      if (lastPoll == state) return;
      publish_state(state);
      lastPoll = state;
    }
};
