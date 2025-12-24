from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options


options = Options()
options.add_argument("--headless")
driver = webdriver.Chrome(options=options)
driver.delete_all_cookies()

with open('db.txt', 'w', encoding='utf-8') as db_f:
    with open('link.txt', 'r') as f:
        for line in f:
            line = line.strip()
            line = line[:-1]
            url = 'https://www.litres.ru/  ' + line
            driver.get(url)

            count_new_comms = 0
            while True and count_new_comms < 3:
                count_new_comms += 1
                try:
                    button = driver.find_element(By.XPATH, "//button[@data-testid='review__addMore--button']")
                    driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", button)
                    button.click()
                except:
                    break

            while True:
                try:
                    button = driver.find_element(By.XPATH, "//a[contains(text(), 'Далее')]")
                    driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", button)
                    button.click()
                except:
                    break

            reviews = driver.find_elements(By.CSS_SELECTOR, 'div[data-testid="review__text"]')
            reviews_list = '['
            for r in reviews:
                reviews_list += r.text.replace('\n', ' ') + ','
            reviews_list = reviews_list[:-1]
            reviews_list += ']'

            title = ''
            descr = ''
            try:
                title = driver.find_elements(By.CSS_SELECTOR, 'h1[itemprop="name"]')[0].text
                title = title.replace('\n', ' ')
                descr = driver.find_element(By.XPATH, '//div[@data-testid="book__infoAboutBook--wrapper"]').text
                descr = descr.replace('\n', ' ')

            except:
                title = ''
                descr = ''

            db_f.write(title + ';' + descr + ';' + reviews_list + '\n')

driver.quit()
