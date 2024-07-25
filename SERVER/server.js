import express from 'express';
import mongoose from 'mongoose';
import mqtt from 'mqtt';
import shortid from 'shortid';
import path from 'path';
import dotenv from 'dotenv';
import moment from 'moment';
import bodyParser from "body-parser";
import { Fan, Led, Door, Sensor } from "./event.js";

const app = express();
dotenv.config();

const DEFAULT_LED_STATE = "off";
const DEFAULT_LED_MODE = 1;

const PORT = process.env.PORT || 7000;
const MONGO_URL = process.env.MONGO_URL;
const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL;
const USER_NAME = process.env.MQTT_USER_NAME;
const PASS_WORD = process.env.MQTT_PASS_WORD;

var last_mode = 1;
var last_state = "off";

const topic = {
    "led": "smarthome/led",
    "door": "smarthome/door",
    "fan": {
        "control": "smarthome/fan/control",
        "time": "smarthome/fan/time"
    },
    "sensor": "smarthome/sensor"
};

// CONNECTION INIT
// MQTT connection
const mqttClient = mqtt.connect(MQTT_BROKER_URL, {
    username: USER_NAME,
    password: PASS_WORD,
    reconnectPeriod: 2000,
});

mongoose.connect(MONGO_URL, {
    useNewUrlParser: true,
    useUnifiedTopology: true
}).then(() => {
    console.log('MongoDB connected');
}).catch((err) => {
    console.log('Error connecting to MongoDB:', err);
});

mqttClient.on('connect', () => {
    console.log('MQTT Connected');

    // Lặp qua tất cả các topic và subscribe
    for (const key in topic) {
        if (typeof topic[key] === 'object') {
            for (const subKey in topic[key]) {
                mqttClient.subscribe(topic[key][subKey], (err) => {
                    if (err) {
                        console.error(`Failed to subscribe to ${topic[key][subKey]}`, err);
                    } else {
                        console.log(`Subscribed to ${topic[key][subKey]}`);
                    }
                });
            }
        } else {
            mqttClient.subscribe(topic[key], (err) => {
                if (err) {
                    console.error(`Failed to subscribe to ${topic[key]}`, err);
                } else {
                    console.log(`Subscribed to ${topic[key]}`);
                }
            });
        }
    }
});

//Khi dữ liệu nhận được trên MQTT
mqttClient.on('message', async (receivedTopic, message) => {
    console.log(`Nhận được tin nhắn từ ${receivedTopic}: ${message.toString()}`);
    let data;
    const receivedMessage = message.toString().split(' ');
    let validCommands;
    let validValues;


    try {
        switch (receivedTopic) {
            case 'smarthome/led':
                const [location, command, value] = receivedMessage;
                validCommands = ['state', 'mode'];
                validValues = ['on', 'off', '1', '2', '3'];

                if (!validCommands.includes(command) || !validValues.includes(value)) {
                    console.log('Dữ liệu cho Led không hợp lệ:');
                    return;
                }

                // Tìm dữ liệu gần nhất cho cả state và mode
                let latestData = await getLatestData(Led, { location });

                if (!latestData) {
                    latestData = { location, state: DEFAULT_LED_STATE, mode: DEFAULT_LED_MODE };
                }
                // Cập nhật state hoặc mode nếu có thay đổi
                if (command === 'state') {
                    last_state = value;
                    last_mode = latestData.mode;
                } else if (command === 'mode') {
                    last_state = latestData.state;
                    last_mode = value;
                } else {
                    console.error('Dữ liệu không hợp lệ:', message.toString());
                    return;
                }

                // Chuẩn bị dữ liệu để lưu
                const ledData = { location, state: last_state, mode: last_mode };

                // Lưu dữ liệu
                await saveData(Led, ledData);
                break;

            case 'smarthome/fan/control':
                try {
                    const speed = parseInt(receivedMessage, 10); // Chuyển đổi dữ liệu nhận được thành số nguyên

                    if (isNaN(speed) || speed < 0 || speed > 100) {
                        console.log('Dữ liệu cho Fan không hợp lệ:');
                        return;
                    }

                    // Nếu dữ liệu là hợp lệ, tiếp tục lưu vào cơ sở dữ liệu
                    data = { speed };
                    await saveData(Fan, data);
                } catch (error) {
                    console.log(`Dữ liệu cho Fan không hợp lệ: `, message.toString());
                }
                break;
            case 'smarthome/door':
                try {
                    const [state] = receivedMessage; // Sử dụng 'state' thay vì 'State'

                    validValues = ['open', 'close']; // Đảm bảo khai báo 'validValues' là const

                    if (!validValues.includes(state)) {
                        console.log('Dữ liệu cho Door không hợp lệ');
                        return;
                    }

                    data = { state };
                    await saveData(Door, data);
                } catch (error) {
                    console.log(`Dữ liệu cho Door không hợp lệ: `, message.toString());
                }
                break;

            case 'smarthome/sensor':
                data = JSON.parse(message.toString());
                data.created = moment().utc().add(5, 'hours');
                await saveData(Sensor, data);
                break;

            default:
                console.error('Chủ đề không xác định:', receivedTopic);
        }
    } catch (error) {
        console.error('Lỗi khi phân tích cú pháp hoặc lưu dữ liệu:', error);
    }
});

// Hàm save upload data lên Mongodb 
// Model sẽ phụ thuộc vào topic 
const saveData = async (Model, data) => {
    try {
        const eventData = new Model(data);
        const savedData = await eventData.save();
        // Loại bỏ trường '_id' trước khi hiển thị dữ liệu
        const savedDataWithoutId = savedData.toObject();
        delete savedDataWithoutId._id; // Xóa trường '_id'
        console.log(`${Model.modelName} data : `, savedDataWithoutId);
    } catch (error) {
        console.error('Error saving data:', error);
    }
};

async function getLatestData(Model, query) {
    try {
        let latestData = await Model.findOne(query).sort({ updatedAt: -1 });
        return latestData;
    } catch (error) {
        console.error('Error fetching latest data:', error);
        throw error;
    }
}

app.listen(PORT, () => {
    console.log(`Server is running at http://localhost:${PORT}`);
});
