

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "mqtt_client.h"
#include "esp_http_server.h"

// RFID
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"

#include "mbedtls/sha256.h"

// ================= CERTS =================
extern const uint8_t AmazonRootCA_pem_start[] asm("_binary_AmazonRootCA_pem_start");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");

// ================= CONFIG =================
#define WIFI_SSID "Uilatech"
#define WIFI_PASS "Uilatech*123"

#define MQTT_URI "mqtts://a2qjty4wsdd1fv-ats.iot.us-east-1.amazonaws.com:8883"

#define PUB_TOPIC "esp32/to_aws"
#define SUB_TOPIC "esp32/from_aws"

static const char *TAG = "RFID_SYSTEM";

// ================= GLOBAL =================
esp_mqtt_client_handle_t client;

static char latest_uid[65] = "---";
static char latest_status[32] = "WAITING";

static char employees_json[8192] =
"{\"employees\":[]}";

static char previous_uid[65] = "";




const char* page =

"<!DOCTYPE html><html><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"

"<title>RFID Dashboard</title>"

"<style>"

// ================= LOGIN =================
".login-container{"
"display:flex;"
"justify-content:center;"
"align-items:center;"
"height:100vh;"
"}"

".login-box{"
"background:#1e293b;"
"padding:40px;"
"border-radius:15px;"
"width:350px;"
"box-shadow:0 4px 15px rgba(0,0,0,0.4);"
"text-align:center;"
"}"

".login-box h2{"
"color:#38bdf8;"
"margin-bottom:20px;"
"}"

".login-box input{"
"width:100%;"
"padding:12px;"
"margin-bottom:15px;"
"border:none;"
"border-radius:8px;"
"}"

".login-error{"
"color:#ef4444;"
"margin-top:10px;"
"font-weight:bold;"
"}"

// ================= BODY =================
"body{"
"background:#0f172a;"
"font-family:Arial,sans-serif;"
"color:white;"
"margin:0;"
"padding:20px;"
"}"

"h1{"
"color:#38bdf8;"
"margin-bottom:20px;"
"}"

// ================= STATS =================
".stats{"
"display:flex;"
"gap:20px;"
"margin-bottom:20px;"
"flex-wrap:wrap;"
"}"

".stat-card{"
"background:#1e293b;"
"padding:20px;"
"border-radius:12px;"
"min-width:180px;"
"text-align:center;"
"box-shadow:0 4px 10px rgba(0,0,0,0.3);"
"}"

".stat-card h3{"
"margin:0;"
"color:#94a3b8;"
"}"

".stat-card p{"
"font-size:28px;"
"font-weight:bold;"
"color:#38bdf8;"
"}"

// ================= CONTAINER =================
".container{"
"display:flex;"
"gap:20px;"
"flex-wrap:wrap;"
"}"

".card{"
"background:#1e293b;"
"padding:20px;"
"border-radius:12px;"
"box-shadow:0 4px 10px rgba(0,0,0,0.3);"
"flex:1;"
"min-width:320px;"
"}"

// ================= BUTTON =================
"button{"
"background:#38bdf8;"
"color:white;"
"border:none;"
"padding:10px 15px;"
"border-radius:8px;"
"cursor:pointer;"
"margin:5px;"
"font-weight:bold;"
"}"

"button:hover{"
"background:#0ea5e9;"
"}"

// ================= EMPLOYEE BUTTON =================
".employee-btn{"
"display:block;"
"width:100%;"
"text-align:left;"
"margin-bottom:10px;"
"padding:12px;"
"background:#38bdf8;"
"border:none;"
"border-radius:8px;"
"color:white;"
"font-weight:bold;"
"cursor:pointer;"
"}"

".employee-btn:hover{"
"background:#0ea5e9;"
"}"

// ================= EMPLOYEE LIST =================
"#list{"
"max-height:500px;"
"overflow-y:auto;"
"padding-right:5px;"
"}"

// ================= STATUS =================
".status{"
"padding:10px;"
"border-radius:8px;"
"font-weight:bold;"
"margin-top:10px;"
"}"

".success{"
"background:#166534;"
"}"

".warning{"
"background:#92400e;"
"}"

".error{"
"background:#991b1b;"
"}"

".info{"
"background:#1d4ed8;"
"}"

// ================= INPUT =================
"input{"
"width:100%;"
"padding:10px;"
"border-radius:8px;"
"margin-bottom:15px;"
"border:none;"
"outline:none;"
"}"

"</style>"
"</head>"

"<body>"

// ================= LOGIN PAGE =================
"<div id='loginPage' class='login-container'>"

"<div class='login-box'>"

"<h2>Secure RFID Login</h2>"

"<input type='text' id='username' "
"placeholder='Username'>"

"<input type='password' id='password' "
"placeholder='Password'>"

"<button onclick='login()'>"
"Login"
"</button>"

"<div id='loginError' class='login-error'></div>"

"</div>"

"</div>"

// ================= DASHBOARD =================
"<div id='dashboard' style='display:none;'>"

"<h1>RFID ONBOARDING DASHBOARD</h1>"

// ================= STATS =================
"<div class='stats'>"

"<div class='stat-card'>"
"<h3>Total Employees</h3>"
"<p id='total'>0</p>"
"</div>"

"<div class='stat-card'>"
"<h3>Pending</h3>"
"<p id='pending'>0</p>"
"</div>"

"<div class='stat-card'>"
"<h3>Onboarded</h3>"
"<p id='onboarded'>0</p>"
"</div>"

"</div>"

// ================= FETCH BUTTON =================
"<button onclick='loadEmployees()'>"
"Fetch Employees"
"</button>"

// ================= MAIN DASHBOARD =================
"<div class='container'>"

// ================= EMPLOYEE CARD =================
"<div class='card'>"

"<h2>Pending Employees</h2>"

"<input type='text' "
"id='search' "
"placeholder='Search Employee...' "
"onkeyup='filterEmployees()'>"

"<div id='list'></div>"

"</div>"

// ================= EMPLOYEE DETAILS =================
"<div class='card'>"

"<h2>Employee Details</h2>"

"<p><b>ID:</b> "
"<span id='empid'>---</span></p>"

"<p><b>Name:</b> "
"<span id='empname'>---</span></p>"

"<p><b>Department:</b> "
"<span id='empdept'>---</span></p>"

"<p><b>Role:</b> "
"<span id='emprole'>---</span></p>"

"</div>"

// ================= STATUS CARD =================
"<div class='card'>"

"<h2>Live Status</h2>"

"<p><b>Selected Employee:</b></p>"
"<p id='selected'>NONE</p>"

"<button onclick='assignRFID()'>"
"Assign RFID"
"</button>"

"<hr>"

"<p><b>Latest UID:</b></p>"
"<p id='uid'>---</p>"

"<p><b>Status:</b></p>"

"<div id='statusBox' class='status info'>"
"<span id='status'>WAITING</span>"
"</div>"

"</div>"

"</div>"

"<script>"

// ================= LOGIN =================
"const ADMIN_USER='admin';"

"const ADMIN_PASS='RFID@2026_SECURE';"

"function login()"
"{"

"let user="
"document.getElementById('username').value;"

"let pass="
"document.getElementById('password').value;"

"if(user===ADMIN_USER && pass===ADMIN_PASS)"
"{"

"document.getElementById('loginPage')"
".style.display='none';"

"document.getElementById('dashboard')"
".style.display='block';"

"loadEmployees();"

"}"

"else"
"{"

"document.getElementById('loginError')"
".innerText='Invalid Username or Password';"

"}"

"}"

// ================= GLOBAL =================
"let selectedEmployee='';"
"let employeesData=[];"

// ================= LOAD EMPLOYEES =================
"function loadEmployees(){"

"fetch('/get')"

".then(()=>{"

"setTimeout(()=>{"

"fetch('/employees')"

".then(r=>r.json())"

".then(d=>{"

"console.log(d);"

"employeesData=d.employees;"

// ================= STATS =================
"let total=d.employees.length;"
"let pending=0;"
"let onboarded=0;"

"d.employees.forEach(e=>{"

"if(e.uid==null || e.uid=='')"
"{"
"pending++;"
"}"

"else"
"{"
"onboarded++;"
"}"

"});"

"document.getElementById('total').innerText=total;"
"document.getElementById('pending').innerText=pending;"
"document.getElementById('onboarded').innerText=onboarded;"

// ================= EMPLOYEE LIST =================
"let list=document.getElementById('list');"

"list.innerHTML='';"

"d.employees.forEach(e=>{"

"if(e.uid == null || e.uid == ''){"

"let btn=document.createElement('button');"

"btn.className='employee-btn';"

"btn.innerText=e.name+' ('+e.employee_id+')';"

"btn.onclick=function(){"
"selectEmployee(e.employee_id);"
"};"

"list.appendChild(btn);"

"}"

"});"

"});"

"},1000);"

"});"

"}"

// ================= SELECT EMPLOYEE =================
"function selectEmployee(empid)"
"{"

"selectedEmployee = empid;"

"document.getElementById('selected').innerText = empid;"

"let emp = null;"

"for(let i = 0; i < employeesData.length; i++)"
"{"

"if(employeesData[i].employee_id == empid)"
"{"

"emp = employeesData[i];"

"break;"

"}"

"}"

"if(emp)"
"{"

"document.getElementById('empid').innerText = "
"emp.employee_id ? emp.employee_id : '---';"

"document.getElementById('empname').innerText = "
"emp.name ? emp.name : '---';"

"document.getElementById('empdept').innerText = "
"emp.department ? emp.department : '---';"

"document.getElementById('emprole').innerText = "
"emp.role ? emp.role : '---';"

"}"

// ================= BUTTON HIGHLIGHT =================
"let buttons = "
"document.getElementsByClassName('employee-btn');"

"for(let i = 0; i < buttons.length; i++)"
"{"

"buttons[i].style.background = '#38bdf8';"

"}"

"for(let i = 0; i < buttons.length; i++)"
"{"

"if(buttons[i].innerText.includes(empid))"
"{"

"buttons[i].style.background = '#22c55e';"

"}"

"}"

"}"

// ================= SEARCH =================
"function filterEmployees()"
"{"

"let input="
"document.getElementById('search')"
".value.toLowerCase();"

"let buttons="
"document.getElementsByClassName("
"'employee-btn');"

"for(let i=0;i<buttons.length;i++)"
"{"

"let txt="
"buttons[i].innerText.toLowerCase();"

"buttons[i].style.display="
"txt.includes(input)"
"? 'block'"
": 'none';"

"}"

"}"

// ================= ASSIGN RFID =================
"function assignRFID(){"

"if(selectedEmployee==''){"

"alert('Please select employee');"

"return;"

"}"

"let uid=document.getElementById('uid').innerText;"

"if(uid=='---'){"

"alert('Please scan RFID card');"

"return;"

"}"

"fetch('/assign',{"

"method:'POST',"

"headers:{"
"'Content-Type':'application/json'"
"},"

"body:JSON.stringify({"
"'type':'ASSIGN_UID',"
"'employee_id':selectedEmployee,"
"'uid':uid"
"})"

"})"

".then(r=>r.text())"

".then(d=>{"

"alert(d);"

"loadEmployees();"

"});"

"}"

// ================= LIVE STATUS =================
"setInterval(()=>{"

"fetch('/data')"

".then(r=>r.json())"

".then(d=>{"

"document.getElementById('uid').innerText=d.uid;"

"document.getElementById('status').innerText=d.status;"

"let box=document.getElementById('statusBox');"

"box.className='status info';"

"if(d.status.includes('SUCCESS')){"
"box.className='status success';"
"}"

"else if(d.status.includes('ALREADY')){"
"box.className='status error';"
"}"

"else if(d.status.includes('READY')){"
"box.className='status warning';"
"}"

"});"

"},2000);"

"</script>"

"</div>"

"</body></html>";
// ================= HTTP ROOT =================
esp_err_t root(httpd_req_t *req)
{
    return httpd_resp_send(req,
                           page,
                           HTTPD_RESP_USE_STRLEN);
}

// ================= FETCH EMPLOYEES =================
esp_err_t get(httpd_req_t *req)
{
    esp_mqtt_client_publish(client,
                            PUB_TOPIC,
                            "{\"action\":\"GET_EMPLOYEES\"}",
                            0,
                            1,
                            0);

    return httpd_resp_send(req,
                           "OK",
                           HTTPD_RESP_USE_STRLEN);
}

// ================= EMPLOYEE JSON =================
esp_err_t employees(httpd_req_t *req)
{
    httpd_resp_set_type(req,
                        "application/json");

    return httpd_resp_send(req,
                           employees_json,
                           HTTPD_RESP_USE_STRLEN);
}

// ================= LIVE DATA =================
esp_err_t data(httpd_req_t *req)
{
    char resp[256];

    snprintf(resp,
             sizeof(resp),
             "{\"uid\":\"%s\",\"status\":\"%s\"}",
             latest_uid,
             latest_status);

    httpd_resp_set_type(req,
                        "application/json");

    return httpd_resp_send(req,
                           resp,
                           HTTPD_RESP_USE_STRLEN);
}

// ================= ASSIGN RFID =================
esp_err_t assign(httpd_req_t *req)
{
    char buf[512];

    memset(buf,
           0,
           sizeof(buf));

    int total_len =
        req->content_len;

    int cur_len = 0;

    int received = 0;

    // RECEIVE FULL POST DATA
    while (cur_len < total_len)
    {
        received = httpd_req_recv(
                        req,
                        buf + cur_len,
                        total_len - cur_len);

        if (received <= 0)
        {
            printf("POST RECEIVE FAILED\n");

            return ESP_FAIL;
        }

        cur_len += received;
    }

    buf[cur_len] = '\0';

    printf("\n========================\n");
    printf("ASSIGN REQUEST RECEIVED\n");
    printf("%s\n", buf);
    printf("========================\n");

    // SEND TO AWS
    esp_mqtt_client_publish(client,
                            PUB_TOPIC,
                            buf,
                            0,
                            1,
                            0);

    printf("ASSIGN MQTT SENT\n");

    httpd_resp_set_type(req,
                        "text/plain");

    return httpd_resp_send(req,
                           "RFID ASSIGN SENT TO AWS",
                           HTTPD_RESP_USE_STRLEN);
}

// ================= WEB SERVER =================
void start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    if (httpd_start(&server,&config) == ESP_OK)
    {
        httpd_uri_t u1 =
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root
        };

        httpd_uri_t u2 =
        {
            .uri = "/get",
            .method = HTTP_GET,
            .handler = get
        };

        httpd_uri_t u3 =
        {
            .uri = "/employees",
            .method = HTTP_GET,
            .handler = employees
        };

        httpd_uri_t u4 =
        {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = data
        };

        httpd_uri_t u5 =
        {
            .uri = "/assign",
            .method = HTTP_POST,
            .handler = assign
        };

        httpd_register_uri_handler(server, &u1);
        httpd_register_uri_handler(server, &u2);
        httpd_register_uri_handler(server, &u3);
        httpd_register_uri_handler(server, &u4);
        httpd_register_uri_handler(server, &u5);

        printf("HTTP SERVER STARTED\n");
    }
}

// ================= MQTT =================
static void mqtt_handler(void *handler_args,
                         esp_event_base_t base,
                         int32_t event_id,
                         void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    if (event_id == MQTT_EVENT_CONNECTED)
    {
        printf("MQTT CONNECTED\n");

        esp_mqtt_client_subscribe(client,
                                  SUB_TOPIC,
                                  1);
    }

    else if (event_id == MQTT_EVENT_DATA)
    {
        static char full_msg[8192];

        memcpy(full_msg + event->current_data_offset,
               event->data,
               event->data_len);

        if (event->current_data_offset +
            event->data_len ==
            event->total_data_len)
        {
            full_msg[event->total_data_len] = '\0';

            printf("\nFULL JSON RECEIVED:\n%s\n",
                   full_msg);

            // EMPLOYEE LIST
            if (strstr(full_msg,
                       "EMPLOYEE_LIST"))
            {
                strncpy(employees_json,
                        full_msg,
                        sizeof(employees_json) - 1);
            }

            // STATUS
            if (strstr(full_msg,
                       "STATUS"))
            {
                char *uid_ptr =
                    strstr(full_msg,
                           "\"uid\":");

                char *msg_ptr =
                    strstr(full_msg,
                           "\"message\":");

                if (uid_ptr && msg_ptr)
                {
                    sscanf(uid_ptr,
                           "\"uid\": \"%[^\"]",
                           latest_uid);

                    sscanf(msg_ptr,
                           "\"message\": \"%[^\"]",
                           latest_status);

                    printf("UPDATED → UID: %s | STATUS: %s\n",
                           latest_uid,
                           latest_status);
                }
            }

            memset(full_msg,
                   0,
                   sizeof(full_msg));
        }
    }
}

// ================= MQTT START =================
void mqtt_start()
{
    esp_mqtt_client_config_t cfg =
    {
        .broker.address.uri = MQTT_URI,

        .broker.verification.certificate =
            (const char *)AmazonRootCA_pem_start,

        .credentials.authentication.certificate =
            (const char *)certificate_pem_crt_start,

        .credentials.authentication.key =
            (const char *)private_pem_key_start,
    };

    client = esp_mqtt_client_init(&cfg);

    esp_mqtt_client_register_event(client,
                                   ESP_EVENT_ANY_ID,
                                   mqtt_handler,
                                   NULL);

    esp_mqtt_client_start(client);
}

// ================= WIFI =================
void wifi_init()
{
    esp_netif_init();

    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&cfg);

    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_wifi_set_config(WIFI_IF_STA,
                        &wifi_config);

    esp_wifi_start();

    esp_wifi_connect();

    printf("WiFi Connected\n");
}

// ================= SHA256 =================
void sha256_hash(const char *input,
                 char *output)
{
    unsigned char hash[32];

    mbedtls_sha256((const unsigned char *)input,
                   strlen(input),
                   hash,
                   0);

    for (int i = 0; i < 32; i++)
    {
        sprintf(output + (i * 2),
                "%02x",
                hash[i]);
    }

    output[64] = '\0';
}

// ================= RFID =================
#define RC522_SPI_BUS_GPIO_MISO 19
#define RC522_SPI_BUS_GPIO_MOSI 23
#define RC522_SPI_BUS_GPIO_SCLK 18

#define RC522_SPI_SCANNER_GPIO_SDA 5
#define RC522_SCANNER_GPIO_RST 22

static void on_card(void *arg,
                    esp_event_base_t base,
                    int32_t event_id,
                    void *data)
{
    rc522_picc_state_changed_event_t *event = data;

    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE)
    {
        char uid_str[32] = {0};

        // READ UID
        for (int i = 0; i < picc->uid.length; i++)
        {
            sprintf(uid_str + strlen(uid_str),"%02X",picc->uid.value[i]);
        }

        // HASH UID
        char hashed_uid[65] = {0};

        sha256_hash(uid_str,hashed_uid);

        // PREVENT REPEATED SAME CARD
        if(strcmp(previous_uid,
                  hashed_uid) == 0)
        {
            return;
        }

        strcpy(previous_uid,
               hashed_uid);

        printf("\nCARD DETECTED\n");
        printf("RAW UID  : %s\n", uid_str);
        printf("HASH UID : %s\n", hashed_uid);

        // UPDATE WEBPAGE
        strcpy(latest_uid,
               hashed_uid);

        strcpy(latest_status,
               "CARD_SCANNED");

        // BUILD JSON
        char msg[256];

        snprintf(msg,
                 sizeof(msg),
                 "{\"type\":\"RFID_SCAN\",\"uid\":\"%s\"}",
                 hashed_uid);

        printf("MQTT PAYLOAD:\n%s\n",
               msg);

        // SEND TO AWS
        esp_mqtt_client_publish(client,PUB_TOPIC,msg,0,1,0);

        printf("MQTT SENT TO AWS\n");
    }
}

// ================= RFID INIT =================
void rfid_init()
{
    rc522_spi_config_t driver_config =
    {
        .host_id = SPI3_HOST,

        .bus_config = &(spi_bus_config_t)
        {
            .miso_io_num =
                RC522_SPI_BUS_GPIO_MISO,

            .mosi_io_num =
                RC522_SPI_BUS_GPIO_MOSI,

            .sclk_io_num =
                RC522_SPI_BUS_GPIO_SCLK,
        },

        .dev_config =
        {
            .spics_io_num =
                RC522_SPI_SCANNER_GPIO_SDA,
        },

        .rst_io_num =
            RC522_SCANNER_GPIO_RST,
    };

    rc522_driver_handle_t driver;

    rc522_spi_create(&driver_config,
                     &driver);

    rc522_driver_install(driver);

    rc522_config_t scanner_config =
    {
        .driver = driver,
    };

    rc522_handle_t scanner;

    rc522_create(&scanner_config,
                 &scanner);

    rc522_register_events(scanner,
                          RC522_EVENT_PICC_STATE_CHANGED,
                          on_card,
                          NULL);

    rc522_start(scanner);
}

// ================= MAIN =================
void app_main(void)
{
    nvs_flash_init();

    wifi_init();

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    start_webserver();

    mqtt_start();

    rfid_init();

    printf("SYSTEM READY\n");
}
