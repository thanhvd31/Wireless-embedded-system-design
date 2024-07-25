import mongoose from 'mongoose';
import moment from 'moment';

const Schema = mongoose.Schema;


const SensorSchema = new Schema({
    Temperature: {
        type: Number,
        required: true
    },

    Humidity: {
        type: Number,
        required: true
    },

    created: {
        type: Date,
        default: moment().utc().add(6, 'hours')
    }
}, { versionKey: false }
);

const DoorSchema = new Schema({
    state: String,
    updatedAt: { type: Date, default: Date.now }
}, { versionKey: false });


const LedSchema = new mongoose.Schema({
    location: String,
    state: String,
    mode: Number,
    updatedAt: { type: Date, default: Date.now }
}, { versionKey: false });

const FanSchema = new Schema({
    speed: String,
    updatedAt: { type: Date, default: Date.now }
}, { versionKey: false });

LedSchema.pre('save', function (next) {
    this.updatedAt = Date.now();
    next();
});

DoorSchema.pre('save', function (next) {
    this.updatedAt = Date.now();
    next();
});

FanSchema.pre('save', function (next) {
    this.updatedAt = Date.now();
    next();
});



const Fan = mongoose.model('Fan', FanSchema);
const Led = mongoose.model('Led', LedSchema);
const Door = mongoose.model('Door', DoorSchema);
const Sensor = mongoose.model('Sensor', SensorSchema);

export { Fan, Led, Door, Sensor };