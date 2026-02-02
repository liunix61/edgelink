function jsonDeepClone(v) { 
    return JSON.parse(JSON.stringify(v));
}

function cloneMsg(v) {
    var newMsg = jsonDeepClone(v);
    newMsg.id = evalEnv.generateMsgID();
    return newMsg;
}

const node = {
    id: evalEnv.nodeID,
    name: evalEnv.nodeName,
};

const RED = {
    uitl: {
        cloneMessage: function (msg) {
            return cloneMsg(msg);
        },
    }
};