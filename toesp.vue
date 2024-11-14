<!-- WiFi 同步卡片 -->
<div class="card" v-if="currentStep === 2">
    <h2><i class="fas fa-wifi btn-icon"></i> WiFi 同步</h2>
    
    <!-- WiFi列表 -->
    <div class="wifi-list" v-if="wifiList.length > 0">
        <div 
            v-for="wifi in wifiList" 
            :key="wifi.ssid"
            class="wifi-item"
            @click="selectWifi(wifi)"
            :class="{ 'selected': wifi.ssid === ssid }"
        >
            <div class="wifi-info">
                <span class="wifi-name">{{ wifi.ssid }}</span>
                <div class="signal-strength">
                    <i class="fas" :class="getSignalIcon(wifi.rssi)"></i>
                    <span class="signal-text">{{ getSignalStrength(wifi.rssi) }}</span>
                </div>
            </div>
        </div>
    </div>
    
    <!-- WiFi输入框 -->
    <div style="display: flex;justify-content: center;align-items: center;">
        <div class="InputContainer">
            <input v-model="ssid" placeholder="WiFi 名称" class="input" type="text">
        </div>
        <div class="InputContainer">
            <input v-model="password" placeholder="WiFi 密码" class="input" type="password">
        </div>
    </div>

    <ButtonStyle1 
        class="primary-button" 
        :thetext="wifiLoading ? '配网中...' : '同步'" 
        @click="setWifiInfo()"
        :disabled="wifiLoading"
    ></ButtonStyle1>

    <!-- 状态提示 -->
    <p class="status-message" v-if="wifiStatus">{{ wifiStatus }}</p>
    <p class="error-message" v-if="errorMsg">{{ errorMsg }}</p>
</div>

<script setup>
import { ref, onMounted } from 'vue';

// 状态变量
const wifiList = ref([]);
const ssid = ref('');
const password = ref('');
const wifiStatus = ref('');
const errorMsg = ref('');
const wifiLoading = ref(false);

// WiFi信号强度计算函数
const getSignalStrength = (rssi) => {
    if (rssi >= -50) return '极好';
    if (rssi >= -60) return '很好';
    if (rssi >= -70) return '好';
    if (rssi >= -80) return '一般';
    return '差';
};

const getSignalIcon = (rssi) => {
    if (rssi >= -50) return 'fa-wifi';
    if (rssi >= -60) return 'fa-wifi';
    if (rssi >= -70) return 'fa-wifi';
    if (rssi >= -80) return 'fa-wifi';
    return 'fa-wifi';
};

// 选择WiFi
const selectWifi = (wifi) => {
    ssid.value = wifi.ssid;
};

// 获取WiFi列表
const getWifiList = async () => {
    try {
        const res = await myFetch('/scan_wifi');
        if (res.success) {
            wifiList.value = res.data;
        }
    } catch (err) {
        errorMsg.value = '获取WiFi列表失败';
    }
};

// 修改后的setWifiInfo函数
const setWifiInfo = async () => {
    if (wifiLoading.value) return;
    if (!ssid.value) {
        errorMsg.value = '请输入WiFi名称';
        return;
    }
    if (!password.value) {
        errorMsg.value = '请输入WiFi密码';
        return;
    }

    wifiLoading.value = true;
    errorMsg.value = '';
    wifiStatus.value = '正在配网...';
    
    try {
        const res = await myFetch(`/set_config?wifi_name=${encodeURIComponent(ssid.value)}&wifi_pwd=${encodeURIComponent(password.value)}`);
        
        if (res.success) {
            wifiStatus.value = '配网成功,设备即将重启';
            currentStep.value++;
        } else {
            errorMsg.value = '配网失败,请检查账号密码';
        }
    } catch (err) {
        errorMsg.value = '配网出错,请重试';
        console.error(err);
    } finally {
        wifiLoading.value = false;
    }
};

onMounted(() => {
    getWifiList();
});
</script>

<style scoped>
/* 添加新样式 */
.wifi-list {
    max-height: 200px;
    overflow-y: auto;
    margin: 10px 0;
    padding: 0 10px;
}

.wifi-item {
    padding: 12px;
    border-radius: 8px;
    margin: 8px 0;
    background: #f5f5f5;
    cursor: pointer;
    transition: all 0.3s ease;
}

.wifi-item:hover {
    background: #e8e8e8;
}

.wifi-item.selected {
    background: #7873f5;
    color: white;
}

.wifi-info {
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.wifi-name {
    font-size: 14px;
    font-weight: 500;
}

.signal-strength {
    display: flex;
    align-items: center;
    gap: 5px;
}

.signal-text {
    font-size: 12px;
    opacity: 0.8;
}

/* 其他样式保持不变 */
</style>

<script setup>
// 修改myFetch函数添加超时处理
const myFetch = (apiName, params = {}) => {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 5000);

    return fetch(domain + apiName, {
        mode: 'cors',
        signal: controller.signal,
        ...params
    })
    .then(res => res.json())
    .finally(() => clearTimeout(timeoutId));
};
</script>

<style scoped>
.error-message {
    color: #ff4444;
    text-align: center;
    margin: 10px 0;
    font-size: 14px;
}

.status-message {
    color: #7873f5;
    text-align: center;
    margin: 10px 0;
    font-size: 14px;
}
</style>