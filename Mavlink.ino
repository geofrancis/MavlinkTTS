
void FetchMavlinkSerial() {
  //Serial.println("*");
  mavlink_message_t msg;
  mavlink_status_t status;
  while (Serial.available()) {
    uint8_t c = Serial.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      switch (msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:  // #0: Heartbeat
          {
            mavlink_heartbeat_t hb;
            mavlink_msg_heartbeat_decode(&msg, &hb);
            if (hb.type == 10) {
              //   Serial.println("***************FC HB******************");
              BASEMODE = (hb.base_mode);
              fcmodein = (hb.custom_mode);
            }
            break;
          }


        case MAVLINK_MSG_ID_GPS_RAW_INT:
          {
            mavlink_gps_raw_int_t packet;
            mavlink_msg_gps_raw_int_decode(&msg, &packet);

            mavlink_gps_raw_int_t datagps;
            mavlink_msg_gps_raw_int_decode(&msg, &datagps);
            // Serial.println("PX HB");
            // Serial.println("GPS Data ");
            //Serial.print("time usec: ");
            // Serial.println(datagps.time_usec);
            //  Serial.print("lat: ");
            //     Serial.println(datagps.lat);
            //GPSLAT = (datagps.lat);
            //Serial.print("lon: ");
            //Serial.println(datagps.lon);
            // GPSLON = (datagps.lon);
            //  Serial.print("alt: ");
            //   Serial.println(datagps.alt);
            //Serial.print("Satelite visible: ");
            //  Serial.println(datagps.satellites_visible);

            satelites = (datagps.satellites_visible);
            if (satelitesold != satelites) {
              char str[8];
              itoa(satelites, str, 10);
              BMP.speak(str);
              satelitesold = satelites;
              HDOP = (datagps.eph);

              break;
            }


            case MAVLINK_MSG_ID_SYS_STATUS:  // #1: SYS_STATUS
              {
                //mavlink_message_t* msg;
                mavlink_sys_status_t sys_status;
                mavlink_msg_sys_status_decode(&msg, &sys_status);
                //Serial.print("PX SYS STATUS: ");
                // Serial.print("[Bat (V): ");
                // Serial.print(sys_status.voltage_battery);
                VOLTS = (sys_status.voltage_battery);
                // Serial.print(sys_status.voltage_battery / 1000);
                //   Serial.print("], [Bat (A): ");
                //   Serial.print(sys_status.current_battery);
                AMPS = (sys_status.current_battery);
                //  Serial.print("], [Comms loss (%): ");
                //  Serial.print(sys_status.drop_rate_comm);
                //comdroprate = (sys_status.drop_rate_comm);
                //      Serial.println("]");
              }


              break;

            case MAVLINK_MSG_ID_ATTITUDE:  // #30
              {

                mavlink_attitude_t attitude;
                mavlink_msg_attitude_decode(&msg, &attitude);
                // Serial.println("PX ATTITUDE");
                // Serial.println(attitude.roll);
                // roll = (attitude.roll);
                //  pitch = (attitude.pitch);
                //  yaw = (attitude.yaw);
                //if (attitude.roll > 1) leds_modo = 0;
                //else if (attitude.roll < -1) leds_modo = 2;
                //else leds_modo = 1;
              }

              break;

            case MAVLINK_MSG_ID_RC_CHANNELS_RAW:  // #35
              {
                mavlink_rc_channels_raw_t chs;
                mavlink_msg_rc_channels_raw_decode(&msg, &chs);
                //  Serial.print("Chanel 1 raw ");
                //   Serial.println(chs.chan1_raw);
              }

              break;

            case MAVLINK_MSG_ID_MISSION_CURRENT:
              {
                mavlink_mission_current_t RPNUM;
                mavlink_msg_mission_current_decode(&msg, &RPNUM);
                //  Serial.print("wp_number ");
                //  Serial.println(wp_number);
                wp_number = (RPNUM.seq);
                if (wp_numberold != wp_number) {
                  BMP.speak("waypoint");
                  char str1[8];
                  itoa(wp_number, str1, 10);
                  BMP.speak(str1);
                  BMP.speak("heading");
                  char str2[8];
                  itoa(navbearing, str2, 10);
                  BMP.speak(str2);
                  BMP.speak("degrees ");
                  BMP.speak(" distance ");
                  char str3[8];
                  itoa(wpdist, str3, 10);
                  BMP.speak(str3);
                  BMP.speak("  ");
                  wp_numberold = wp_number;
                }
              }
          }
          break;

        case MAVLINK_MSG_ID_VFR_HUD:
          {
            mavlink_vfr_hud_t vfrhud;
            mavlink_msg_vfr_hud_decode(&msg, &vfrhud);
            //   Serial.print("Ground Speed: ");
            //   Serial.println(vfrhud.groundspeed);
            ////  Serial.print("Heading ");
            //   Serial.println(vfrhud.heading);
            gps_Vel = vfrhud.groundspeed;
            gps_Head = vfrhud.heading;
          }

          break;
        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
          {
            mavlink_nav_controller_output_t navout;
            mavlink_msg_nav_controller_output_decode(&msg, &navout);
            navbearing = navout.target_bearing;
            wpdist = navout.wp_dist;
            xtrackerror = navout.xtrack_error;
            //    Serial.print("navbearing: ");
            //    Serial.println(navout.nav_bearing);
            // Serial.print("wpdist ");
            //  Serial.println(navout.wp_dist);
            // Serial.print("xtrackerror ");
            // Serial.println(navout.xtrack_error);
          }

          break;


        case MAVLINK_MSG_ID_STATUSTEXT:  //  #253  https://mavlink.io/en/messages/common.html#STATUSTEXT
          {
            mavlink_statustext_t packet;
            mavlink_msg_statustext_decode(&msg, &packet);
            if (packet.severity > 0) {
              // Serial.print("MAVMSG---------------------------------------------------");
              //Serial.print(" severity: ");
              // Serial.print(packet.severity);
              //    Serial.print("     text: ");
              //  Serial.println(packet.text);
              mavmessage = (packet.text);
              if (mavmessageold != mavmessage) {
                BMP.speak(mavmessage);
                mavmessageold = mavmessage;
              }
            }
          }
      }
      break;
    }
  }
}

