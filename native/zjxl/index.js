"use strict";

const path = require("path");

let native;

try {
    native = require(
        path.join(__dirname, "build", "Release", "zjxl.node")
    );
} catch (error) {
    error.message =
        `Failed to load zjxl native addon.\n\n` +
        `Expected:\n` +
        `  ${path.join(__dirname, "build", "Release", "zjxl.node")}\n\n` +
        `Build it with:\n` +
        `npm install\n` +
        `npm run build\n\n` +
        `Original error:\n${error.message}`;

    throw error;
}

module.exports = native;