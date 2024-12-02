<template>
	<div class="setup-wizard">
		<LoadingBook v-if="isloading"></LoadingBook>
		<div class="background-elements">
			<div class="bg-element circuit-1"></div>
			<div class="bg-element circuit-2"></div>
			<div class="bg-element board"></div>
		</div>

		<div class="wizard-container">
			<div class="step-indicator">
				<div class="step" :class="{ active: currentStep === 1 }"></div>
				<div class="step" :class="{ active: currentStep === 2 }"></div>
				<div class="step" :class="{ active: currentStep === 3 }"></div>
			</div>

			<div class="card-container">
				<!-- 蓝牙连接卡片 -->
				<div class="card" v-if="currentStep === 1">
					<h2>连 接</h2>
					<div v-if="connectBlue" class="bluetooth-animation">
						<div class="pulse"></div>
					</div>
					<div  v-else  style="display: flex;justify-content: center;align-items: center; margin: 25px;">
						<div class="loader"></div>
					</div>
					
					<p class="status">{{blueTipText}}</p>
					<ButtonStyle1 class="primary-button" :thetext="blueSubmitButton" @click="submiBlue"></ButtonStyle1>

				</div>

				<!-- WiFi 同步卡片 -->
				<div class="card" v-if="currentStep === 2">
					<h2><i class="fas fa-wifi btn-icon"></i> WiFi 同步</h2>
					<div style=" display: flex;justify-content: center;align-items: center; ">
						<div class="InputContainer">
							<input placeholder="WiFi 名称" id="input" class="input" name="text" type="text">
						</div>
						<div class="InputContainer">
							<input placeholder="WiFi 密码" id="input" class="input" name="text" type="text">

						</div>
					</div>

					<ButtonStyle1 class="primary-button" thetext="同步" @click="setWifiInfo()"></ButtonStyle1>


				</div>

				<!-- 初始化角色卡片 -->
				<div class="card" v-if="currentStep === 3">
					<h2><i class="fas fa-user-circle btn-icon"></i> 加载角色:{{role_name}}</h2>
					<!-- ... 角色选择部分保持不变 ... -->
					<ButtonStyle1 class="primary-button" thetext="确认" @click=""></ButtonStyle1>
				</div>
			</div>

			<div class="navigation-buttons">
				<button class="icon-button" @click="previousStep">
					<i class="fas fa-chevron-left"></i>
				</button>

			</div>
		</div>
	</div>
</template>

<script setup>
	import {
		ref,
		provide,
		onBeforeMount
	} from "vue";
	import LoadingBook from "../../components/LoadingBook.vue";
	import ButtonStyle1 from "../../components/ButtonStyle1.vue";
	const isloading = ref(false);
	const currentStep = ref(1);

	const connectBlue = ref(false)
	const blueTipText = ref("等待连接...")
	const blueSubmitButton = ref("我已连接")
	const domain = '';
	const ssid = ref('');
	const password = ref('');
	const deviceId = ref('-000');
	const wifiLoading = ref(false)
	const role_name = ref("希儿")
	onBeforeMount(() => {
		// 在这里执行代码
		isloading.value = true;
		setTimeout(() => {
			isloading.value = false;
		}, 500);
	});
	const submiBlue = () => {
		if (blueSubmitButton.value === "就是这个设备，我确定") {
			currentStep.value++;
			refreshInfo()
		} else {
			connectBlue.value = true;
			blueTipText.value = `当前设备机器码:${deviceId.value}，是否确定`;
			blueSubmitButton.value = "就是这个设备，我确定";
		}

	};
	const setWifiInfo = () => {
		currentStep.value++;
		return;
		if (wifiLoading.value) return;
		if (!ssid.value) {
			alert('请输入 WIFI 账号哦~');
			return;
		}
		if (!password.value) {
			alert('请输入 WIFI 密码哦~');
			return;
		}
		wifiLoading.value = true;
		clearTimeout(window.reloadTimer);
		window.reloadTimer = setTimeout(() => {
			alert('配网失败，即将重启设备，请刷新页面重新配网。');
			window.location.reload();
		}, 9000);

		myFetch(`/set_config?wifi_name=${ssid.value}&wifi_pwd=${password.value}`, {})
			.then(res => {
				clearTimeout(window.reloadTimer);
				wifiLoading.value = false;
				if (res.success) {
					alert('配网成功，即将重启设备，关闭本页面即可。');
				} else {
					alert('配网失败，请检查账号密码是否正确');
				}
			});
	};
	const myFetch = (apiName, params) => {
		return fetch(domain + apiName, {
				mode: 'cors'
			})
			.then(res => res.json());
	};
	const refreshInfo = () => {
		myFetch('/get_config', {}).then(res => {
			console.log('wifi信息：', res);
			if (res.success) {
				const data = res.data;
				ssid.value = data.wifi_name;
				password.value = data.wifi_pwd;
				deviceId.value = data.device_id;
			} else {
				alert('获取配置失败, 请刷新页面重试');
			}
		});
	}
	const previousStep = () => {
		if (currentStep.value > 1) currentStep.value--;
		else if (currentStep.value === 1) {
			uni.navigateTo({
				url: '../offical/offical', // 目标页面的路径
			});
		}
	};
</script>

<style scoped>
	@import url('https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css');

	.background-elements {
		position: absolute;
		top: 0;
		left: 0;
		right: 0;
		bottom: 0;
		z-index: 0;

	}

	.bg-element {
		position: absolute;
		opacity: 0.1;
	}

	.btn-icon {
		margin-right: 8px;
		/* 增加图标和文字之间的间距 */
	}

	.setup-wizard {
		background-color: #f0f3ff;
		min-height: 100vh;
		display: flex;
		justify-content: center;
		align-items: center;
		font-family: 'Roboto', sans-serif;
		position: relative;
		overflow: hidden;
	}


	.bg-element {
		position: absolute;
		opacity: 0.1;
	}

	.circuit-1,
	.circuit-2 {
		background-image: url('path-to-circuit-pattern.png');
		background-size: contain;
		width: 300px;
		height: 300px;
	}

	.circuit-1 {
		top: -10%;
		left: -5%;
		width: 40%;
		height: 40%;
		background-image: url('../../static/svg/circuit-svgrepo-com.svg');
		background-size: cover;
		transform: rotate(15deg);
	}

	.circuit-2 {
		bottom: -5%;
		right: -5%;
		width: 40%;
		height: 40%;
		background-image: url('../../static/svg/microchip-svgrepo-com.svg');
		background-size: contain;
		background-repeat: no-repeat;
		transform: rotate(-10deg);
	}



	.board {
		top: -10%;
		right: -5%;
		width: 30%;
		height: 60%;
		background-image: url('../../static/svg/circuit-svgrepo-com (1).svg');
		background-size: contain;
		background-repeat: no-repeat;
		opacity: 0.2;
	}

	.wizard-container {
		background-color: white;
		border-radius: 20px;
		box-shadow: 0 10px 30px rgba(120, 115, 245, 0.2);
		padding: 40px;
		width: 90%;
		max-width: 600px;
		z-index: 1;
	}

	.step-indicator {
		display: flex;
		justify-content: center;
		margin-bottom: 30px;
	}

	.step {
		width: 12px;
		height: 12px;
		border-radius: 50%;
		background-color: #e0e0e0;
		margin: 0 8px;
		transition: background-color 0.3s ease;
	}

	.step.active {
		background-color: #7873f5;
	}

	.card {
		background-color: white;
		border: 2px solid #7873f5;
		border-radius: 15px;
		padding: 30px;
		margin-bottom: 30px;
		transition: all 0.3s ease;
		box-shadow:
			0 2px 4px rgba(0, 0, 0, 0.1),
			0 8px 16px rgba(0, 0, 0, 0.1);
	}

	.card:hover {
		box-shadow: 0 8px 16px rgba(0, 0, 0, 0.2);
	}

	.card h2 {
		color: #777;
		margin-bottom: 20px;


		text-align: center;
		display: flex;
		align-items: center;
		justify-content: center;
	}

	

	.bluetooth-animation {
		position: relative;
		width: 100px;
		height: 100px;
		margin: 20px auto;
	}

	.pulse {
		position: absolute;
		top: 50%;
		left: 50%;
		transform: translate(-50%, -50%);
		width: 80px;
		height: 80px;
		border-radius: 50%;
		background-color: rgba(120, 115, 245, 0.2);
		animation: pulse 2s infinite;
	}

	@keyframes pulse {
		0% {
			transform: translate(-50%, -50%) scale(0.8);
			opacity: 0.7;
		}

		70% {
			transform: translate(-50%, -50%) scale(1.2);
			opacity: 0;
		}

		100% {
			transform: translate(-50%, -50%) scale(0.8);
			opacity: 0;
		}
	}

	.icon-chip {
		position: absolute;
		top: 50%;
		left: 50%;
		transform: translate(-50%, -50%);
		font-size: 40px;
		color: #7873f5;
	}

	.status {
		text-align: center;
		color: #999;
		margin-bottom: 20px;
		font-size: 10px;
	}



	.character-selection {
		display: flex;
		justify-content: space-around;
		margin: 20px 0;
	}

	.character-option {
		text-align: center;
		cursor: pointer;
		transition: transform 0.3s ease;
	}

	.character-option:hover {
		transform: scale(1.05);
	}

	.character-avatar {
		width: 80px;
		height: 80px;
		background-size: cover;
		border-radius: 50%;
		margin: 0 auto 10px;
		border: 2px solid #7873f5;
	}

	.char-1 {
		background-image: url('path-to-char-1.png');
	}

	.char-2 {
		background-image: url('path-to-char-2.png');
	}

	.char-3 {
		background-image: url('path-to-char-3.png');
	}

	.primary-button {
		background-color: #7873f5;
		color: white;
		border: none;
		padding: 12px 24px;
		font-size: 18px;
		display: flex;
		align-items: center;
		justify-content: center;
		width: 100%;
		margin-top: 20px;
	}


	.icon-button {
		background-color: #7873f5;
		color: white;
		border: none;
		width: 50px;
		height: 50px;
		border-radius: 50%;
		display: flex;
		justify-content: center;
		align-items: center;
		cursor: pointer;
		transition: background-color 0.3s ease;
	}

	.icon-button:hover {
		background-color: #6560e0;
	}

	.navigation-buttons {
		display: flex;
		justify-content: space-between;
		margin-top: 20px;
	}

	.InputContainer {
		width: 230px;
		height: 50px;
		display: flex;
		align-items: center;
		justify-content: center;
		background: linear-gradient(to bottom, rgb(227, 213, 255), #7873f5);
		border-radius: 30px;
		overflow: hidden;
		cursor: pointer;
		box-shadow: 2px 2px 10px rgba(0, 0, 0, 0.075);
		margin: 20px;
	}

	.input {
		width: 200px;
		height: 40px;
		border: none;
		outline: none;
		caret-color: rgb(255, 81, 0);
		background-color: rgb(255, 255, 255);
		border-radius: 30px;
		padding-left: 15px;
		letter-spacing: 0.8px;
		color: rgb(19, 19, 19);
		font-size: 13.4px;
	}

	@media (max-width: 768px) {
		.wizard-container {
			width: 95%;
			padding: 20px;
		}
	}
	/* From Uiverse.io by bociKond */ 
	.loader {
	  width: 44.8px;
	  height: 44.8px;
	  color: #7873f5;
	  position: relative;
	  background: radial-gradient(11.2px,currentColor 94%,#0000);
	}
	
	.loader:before {
	  content: '';
	  position: absolute;
	  inset: 0;
	  border-radius: 50%;
	  background: radial-gradient(10.08px at bottom right,#0000 94%,currentColor) top    left,
	          radial-gradient(10.08px at bottom left ,#0000 94%,currentColor) top    right,
	          radial-gradient(10.08px at top    right,#0000 94%,currentColor) bottom left,
	          radial-gradient(10.08px at top    left ,#0000 94%,currentColor) bottom right;
	  background-size: 22.4px 22.4px;
	  background-repeat: no-repeat;
	  animation: loader 1.5s infinite cubic-bezier(0.3,1,0,1);
	}
	
	@keyframes loader {
	  33% {
	    inset: -11.2px;
	    transform: rotate(0deg);
	  }
	
	  66% {
	    inset: -11.2px;
	    transform: rotate(90deg);
	  }
	
	  100% {
	    inset: 0;
	    transform: rotate(90deg);
	  }
	}
</style>